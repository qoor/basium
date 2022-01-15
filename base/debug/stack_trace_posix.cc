// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/stack_trace.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "build/build_config.h"

#if !defined(USE_SYMBOLIZE)
#include <cxxabi.h>
#endif
#if !defined(__UCLIBC__) && !defined(_AIX)
#include <execinfo.h>
#endif

#if BUILDFLAG(IS_APPLE)
#include <AvailabilityMacros.h>
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/debug/proc_maps_linux.h"
#endif

#include "base/cfi_buildflags.h"
#include "base/cxx17_backports.h"
#include "base/debug/debugger.h"
#include "base/debug/stack_trace.h"
#include "base/files/scoped_file.h"
#include "base/ignore_result.h"
#include "base/logging.h"
#include "base/memory/free_deleter.h"
#include "base/memory/singleton.h"
#include "base/numerics/safe_conversions.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"

#if defined(USE_SYMBOLIZE)
#include "base/third_party/symbolize/symbolize.h"

#if BUILDFLAG(ENABLE_STACK_TRACE_LINE_NUMBERS)
#include "base/debug/dwarf_line_no.h"
#endif

#endif

namespace base {
namespace debug {

namespace {

volatile sig_atomic_t in_signal_handler = 0;

#if !BUILDFLAG(IS_NACL)
bool (*try_handle_signal)(int, siginfo_t*, void*) = nullptr;
#endif

#if !defined(USE_SYMBOLIZE)
// The prefix used for mangled symbols, per the Itanium C++ ABI:
// http://www.codesourcery.com/cxx-abi/abi.html#mangling
const char kMangledSymbolPrefix[] = "_Z";

// Characters that can be used for symbols, generated by Ruby:
// (('a'..'z').to_a+('A'..'Z').to_a+('0'..'9').to_a + ['_']).join
const char kSymbolCharacters[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";
#endif  // !defined(USE_SYMBOLIZE)

#if !defined(USE_SYMBOLIZE)
// Demangles C++ symbols in the given text. Example:
//
// "out/Debug/base_unittests(_ZN10StackTraceC1Ev+0x20) [0x817778c]"
// =>
// "out/Debug/base_unittests(StackTrace::StackTrace()+0x20) [0x817778c]"
void DemangleSymbols(std::string* text) {
  // Note: code in this function is NOT async-signal safe (std::string uses
  // malloc internally).

#if !defined(__UCLIBC__) && !defined(_AIX)
  std::string::size_type search_from = 0;
  while (search_from < text->size()) {
    // Look for the start of a mangled symbol, from search_from.
    std::string::size_type mangled_start =
        text->find(kMangledSymbolPrefix, search_from);
    if (mangled_start == std::string::npos) {
      break;  // Mangled symbol not found.
    }

    // Look for the end of the mangled symbol.
    std::string::size_type mangled_end =
        text->find_first_not_of(kSymbolCharacters, mangled_start);
    if (mangled_end == std::string::npos) {
      mangled_end = text->size();
    }
    std::string mangled_symbol =
        text->substr(mangled_start, mangled_end - mangled_start);

    // Try to demangle the mangled symbol candidate.
    int status = 0;
    std::unique_ptr<char, base::FreeDeleter> demangled_symbol(
        abi::__cxa_demangle(mangled_symbol.c_str(), nullptr, 0, &status));
    if (status == 0) {  // Demangling is successful.
      // Remove the mangled symbol.
      text->erase(mangled_start, mangled_end - mangled_start);
      // Insert the demangled symbol.
      text->insert(mangled_start, demangled_symbol.get());
      // Next time, we'll start right after the demangled symbol we inserted.
      search_from = mangled_start + strlen(demangled_symbol.get());
    } else {
      // Failed to demangle.  Retry after the "_Z" we just found.
      search_from = mangled_start + 2;
    }
  }
#endif  // !defined(__UCLIBC__) && !defined(_AIX)
}
#endif  // !defined(USE_SYMBOLIZE)

class BacktraceOutputHandler {
 public:
  virtual void HandleOutput(const char* output) = 0;

 protected:
  virtual ~BacktraceOutputHandler() = default;
};

#if !defined(__UCLIBC__) && !defined(_AIX)
void OutputPointer(void* pointer, BacktraceOutputHandler* handler) {
  // This should be more than enough to store a 64-bit number in hex:
  // 16 hex digits + 1 for null-terminator.
  char buf[17] = { '\0' };
  handler->HandleOutput("0x");
  internal::itoa_r(reinterpret_cast<intptr_t>(pointer),
                   buf, sizeof(buf), 16, 12);
  handler->HandleOutput(buf);
}

#if defined(USE_SYMBOLIZE)
void OutputFrameId(intptr_t frame_id, BacktraceOutputHandler* handler) {
  // Max unsigned 64-bit number in decimal has 20 digits (18446744073709551615).
  // Hence, 30 digits should be more than enough to represent it in decimal
  // (including the null-terminator).
  char buf[30] = { '\0' };
  handler->HandleOutput("#");
  internal::itoa_r(frame_id, buf, sizeof(buf), 10, 1);
  handler->HandleOutput(buf);
}
#endif  // defined(USE_SYMBOLIZE)

void ProcessBacktrace(void* const* trace,
                      size_t size,
                      const char* prefix_string,
                      BacktraceOutputHandler* handler) {
  // NOTE: This code MUST be async-signal safe (it's used by in-process
  // stack dumping signal handler). NO malloc or stdio is allowed here.

#if defined(USE_SYMBOLIZE)
#if BUILDFLAG(ENABLE_STACK_TRACE_LINE_NUMBERS)
  uint64_t* cu_offsets =
      static_cast<uint64_t*>(alloca(sizeof(uint64_t) * size));
  GetDwarfCompileUnitOffsets(trace, cu_offsets, size);
#endif

  for (size_t i = 0; i < size; ++i) {
    if (prefix_string)
      handler->HandleOutput(prefix_string);

    OutputFrameId(i, handler);
    handler->HandleOutput(" ");
    OutputPointer(trace[i], handler);
    handler->HandleOutput(" ");

    char buf[1024] = {'\0'};

    // Subtract by one as return address of function may be in the next
    // function when a function is annotated as noreturn.
    void* address = static_cast<char*>(trace[i]) - 1;
    if (google::Symbolize(address, buf, sizeof(buf))) {
      handler->HandleOutput(buf);
#if BUILDFLAG(ENABLE_STACK_TRACE_LINE_NUMBERS)
      // Only output the source line number if the offset was found. Otherwise,
      // it takes far too long in debug mode when there are lots of symbols.
      if (GetDwarfSourceLineNumber(address, cu_offsets[i], &buf[0],
                                   sizeof(buf))) {
        handler->HandleOutput(" [");
        handler->HandleOutput(buf);
        handler->HandleOutput("]");
      }
#endif
    } else {
      handler->HandleOutput("<unknown>");
    }

    handler->HandleOutput("\n");
  }
#else
  bool printed = false;

  // Below part is async-signal unsafe (uses malloc), so execute it only
  // when we are not executing the signal handler.
  if (in_signal_handler == 0) {
    std::unique_ptr<char*, FreeDeleter> trace_symbols(
        backtrace_symbols(trace, size));
    if (trace_symbols.get()) {
      for (size_t i = 0; i < size; ++i) {
        std::string trace_symbol = trace_symbols.get()[i];
        DemangleSymbols(&trace_symbol);
        if (prefix_string)
          handler->HandleOutput(prefix_string);
        handler->HandleOutput(trace_symbol.c_str());
        handler->HandleOutput("\n");
      }

      printed = true;
    }
  }

  if (!printed) {
    for (size_t i = 0; i < size; ++i) {
      handler->HandleOutput(" [");
      OutputPointer(trace[i], handler);
      handler->HandleOutput("]\n");
    }
  }
#endif  // defined(USE_SYMBOLIZE)
}
#endif  // !defined(__UCLIBC__) && !defined(_AIX)

void PrintToStderr(const char* output) {
  // NOTE: This code MUST be async-signal safe (it's used by in-process
  // stack dumping signal handler). NO malloc or stdio is allowed here.
  ignore_result(HANDLE_EINTR(write(STDERR_FILENO, output, strlen(output))));
}

void StackDumpSignalHandler(int signal, siginfo_t* info, void* void_context) {
  // NOTE: This code MUST be async-signal safe.
  // NO malloc or stdio is allowed here.

#if !BUILDFLAG(IS_NACL)
  // Give a registered callback a chance to recover from this signal
  //
  // V8 uses guard regions to guarantee memory safety in WebAssembly. This means
  // some signals might be expected if they originate from Wasm code while
  // accessing the guard region. We give V8 the chance to handle and recover
  // from these signals first.
  if (try_handle_signal != nullptr &&
      try_handle_signal(signal, info, void_context)) {
    // The first chance handler took care of this. The SA_RESETHAND flag
    // replaced this signal handler upon entry, but we want to stay
    // installed. Thus, we reinstall ourselves before returning.
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_flags = SA_RESETHAND | SA_SIGINFO;
    action.sa_sigaction = &StackDumpSignalHandler;
    sigemptyset(&action.sa_mask);

    sigaction(signal, &action, nullptr);
    return;
  }
#endif

// Do not take the "in signal handler" code path on Mac in a DCHECK-enabled
// build, as this prevents seeing a useful (symbolized) stack trace on a crash
// or DCHECK() failure. While it may not be fully safe to run the stack symbol
// printing code, in practice it's better to provide meaningful stack traces -
// and the risk is low given we're likely crashing already.
#if !BUILDFLAG(IS_APPLE) || !DCHECK_IS_ON()
  // Record the fact that we are in the signal handler now, so that the rest
  // of StackTrace can behave in an async-signal-safe manner.
  in_signal_handler = 1;
#endif

  if (BeingDebugged())
    BreakDebugger();

  PrintToStderr("Received signal ");
  char buf[1024] = { 0 };
  internal::itoa_r(signal, buf, sizeof(buf), 10, 0);
  PrintToStderr(buf);
  if (signal == SIGBUS) {
    if (info->si_code == BUS_ADRALN)
      PrintToStderr(" BUS_ADRALN ");
    else if (info->si_code == BUS_ADRERR)
      PrintToStderr(" BUS_ADRERR ");
    else if (info->si_code == BUS_OBJERR)
      PrintToStderr(" BUS_OBJERR ");
    else
      PrintToStderr(" <unknown> ");
  } else if (signal == SIGFPE) {
    if (info->si_code == FPE_FLTDIV)
      PrintToStderr(" FPE_FLTDIV ");
    else if (info->si_code == FPE_FLTINV)
      PrintToStderr(" FPE_FLTINV ");
    else if (info->si_code == FPE_FLTOVF)
      PrintToStderr(" FPE_FLTOVF ");
    else if (info->si_code == FPE_FLTRES)
      PrintToStderr(" FPE_FLTRES ");
    else if (info->si_code == FPE_FLTSUB)
      PrintToStderr(" FPE_FLTSUB ");
    else if (info->si_code == FPE_FLTUND)
      PrintToStderr(" FPE_FLTUND ");
    else if (info->si_code == FPE_INTDIV)
      PrintToStderr(" FPE_INTDIV ");
    else if (info->si_code == FPE_INTOVF)
      PrintToStderr(" FPE_INTOVF ");
    else
      PrintToStderr(" <unknown> ");
  } else if (signal == SIGILL) {
    if (info->si_code == ILL_BADSTK)
      PrintToStderr(" ILL_BADSTK ");
    else if (info->si_code == ILL_COPROC)
      PrintToStderr(" ILL_COPROC ");
    else if (info->si_code == ILL_ILLOPN)
      PrintToStderr(" ILL_ILLOPN ");
    else if (info->si_code == ILL_ILLADR)
      PrintToStderr(" ILL_ILLADR ");
    else if (info->si_code == ILL_ILLTRP)
      PrintToStderr(" ILL_ILLTRP ");
    else if (info->si_code == ILL_PRVOPC)
      PrintToStderr(" ILL_PRVOPC ");
    else if (info->si_code == ILL_PRVREG)
      PrintToStderr(" ILL_PRVREG ");
    else
      PrintToStderr(" <unknown> ");
  } else if (signal == SIGSEGV) {
    if (info->si_code == SEGV_MAPERR)
      PrintToStderr(" SEGV_MAPERR ");
    else if (info->si_code == SEGV_ACCERR)
      PrintToStderr(" SEGV_ACCERR ");
    else
      PrintToStderr(" <unknown> ");
  }
  if (signal == SIGBUS || signal == SIGFPE ||
      signal == SIGILL || signal == SIGSEGV) {
    internal::itoa_r(reinterpret_cast<intptr_t>(info->si_addr),
                     buf, sizeof(buf), 16, 12);
    PrintToStderr(buf);
  }
  PrintToStderr("\n");

#if BUILDFLAG(CFI_ENFORCEMENT_TRAP)
  if (signal == SIGILL && info->si_code == ILL_ILLOPN) {
    PrintToStderr(
        "CFI: Most likely a control flow integrity violation; for more "
        "information see:\n");
    PrintToStderr(
        "https://www.chromium.org/developers/testing/control-flow-integrity\n");
  }
#endif  // BUILDFLAG(CFI_ENFORCEMENT_TRAP)

  debug::StackTrace().Print();

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#if ARCH_CPU_X86_FAMILY
  ucontext_t* context = reinterpret_cast<ucontext_t*>(void_context);
  const struct {
    const char* label;
    greg_t value;
  } registers[] = {
#if ARCH_CPU_32_BITS
    { "  gs: ", context->uc_mcontext.gregs[REG_GS] },
    { "  fs: ", context->uc_mcontext.gregs[REG_FS] },
    { "  es: ", context->uc_mcontext.gregs[REG_ES] },
    { "  ds: ", context->uc_mcontext.gregs[REG_DS] },
    { " edi: ", context->uc_mcontext.gregs[REG_EDI] },
    { " esi: ", context->uc_mcontext.gregs[REG_ESI] },
    { " ebp: ", context->uc_mcontext.gregs[REG_EBP] },
    { " esp: ", context->uc_mcontext.gregs[REG_ESP] },
    { " ebx: ", context->uc_mcontext.gregs[REG_EBX] },
    { " edx: ", context->uc_mcontext.gregs[REG_EDX] },
    { " ecx: ", context->uc_mcontext.gregs[REG_ECX] },
    { " eax: ", context->uc_mcontext.gregs[REG_EAX] },
    { " trp: ", context->uc_mcontext.gregs[REG_TRAPNO] },
    { " err: ", context->uc_mcontext.gregs[REG_ERR] },
    { "  ip: ", context->uc_mcontext.gregs[REG_EIP] },
    { "  cs: ", context->uc_mcontext.gregs[REG_CS] },
    { " efl: ", context->uc_mcontext.gregs[REG_EFL] },
    { " usp: ", context->uc_mcontext.gregs[REG_UESP] },
    { "  ss: ", context->uc_mcontext.gregs[REG_SS] },
#elif ARCH_CPU_64_BITS
    { "  r8: ", context->uc_mcontext.gregs[REG_R8] },
    { "  r9: ", context->uc_mcontext.gregs[REG_R9] },
    { " r10: ", context->uc_mcontext.gregs[REG_R10] },
    { " r11: ", context->uc_mcontext.gregs[REG_R11] },
    { " r12: ", context->uc_mcontext.gregs[REG_R12] },
    { " r13: ", context->uc_mcontext.gregs[REG_R13] },
    { " r14: ", context->uc_mcontext.gregs[REG_R14] },
    { " r15: ", context->uc_mcontext.gregs[REG_R15] },
    { "  di: ", context->uc_mcontext.gregs[REG_RDI] },
    { "  si: ", context->uc_mcontext.gregs[REG_RSI] },
    { "  bp: ", context->uc_mcontext.gregs[REG_RBP] },
    { "  bx: ", context->uc_mcontext.gregs[REG_RBX] },
    { "  dx: ", context->uc_mcontext.gregs[REG_RDX] },
    { "  ax: ", context->uc_mcontext.gregs[REG_RAX] },
    { "  cx: ", context->uc_mcontext.gregs[REG_RCX] },
    { "  sp: ", context->uc_mcontext.gregs[REG_RSP] },
    { "  ip: ", context->uc_mcontext.gregs[REG_RIP] },
    { " efl: ", context->uc_mcontext.gregs[REG_EFL] },
    { " cgf: ", context->uc_mcontext.gregs[REG_CSGSFS] },
    { " erf: ", context->uc_mcontext.gregs[REG_ERR] },
    { " trp: ", context->uc_mcontext.gregs[REG_TRAPNO] },
    { " msk: ", context->uc_mcontext.gregs[REG_OLDMASK] },
    { " cr2: ", context->uc_mcontext.gregs[REG_CR2] },
#endif  // ARCH_CPU_32_BITS
  };

#if ARCH_CPU_32_BITS
  const int kRegisterPadding = 8;
#elif ARCH_CPU_64_BITS
  const int kRegisterPadding = 16;
#endif

  for (size_t i = 0; i < base::size(registers); i++) {
    PrintToStderr(registers[i].label);
    internal::itoa_r(registers[i].value, buf, sizeof(buf),
                     16, kRegisterPadding);
    PrintToStderr(buf);

    if ((i + 1) % 4 == 0)
      PrintToStderr("\n");
  }
  PrintToStderr("\n");
#endif  // ARCH_CPU_X86_FAMILY
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  PrintToStderr("[end of stack trace]\n");

#if BUILDFLAG(IS_MAC)
  if (::signal(signal, SIG_DFL) == SIG_ERR) {
    _exit(EXIT_FAILURE);
  }
#elif !BUILDFLAG(IS_LINUX)
  // For all operating systems but Linux we do not reraise the signal that
  // brought us here but terminate the process immediately.
  // Otherwise various tests break on different operating systems, see
  // https://code.google.com/p/chromium/issues/detail?id=551681 amongst others.
  PrintToStderr(
      "Calling _exit(EXIT_FAILURE). Core file will not be generated.\n");
  _exit(EXIT_FAILURE);
#endif  // !BUILDFLAG(IS_LINUX)

  // After leaving this handler control flow returns to the point where the
  // signal was raised, raising the current signal once again but executing the
  // default handler instead of this one.
}

class PrintBacktraceOutputHandler : public BacktraceOutputHandler {
 public:
  PrintBacktraceOutputHandler() = default;

  PrintBacktraceOutputHandler(const PrintBacktraceOutputHandler&) = delete;
  PrintBacktraceOutputHandler& operator=(const PrintBacktraceOutputHandler&) =
      delete;

  void HandleOutput(const char* output) override {
    // NOTE: This code MUST be async-signal safe (it's used by in-process
    // stack dumping signal handler). NO malloc or stdio is allowed here.
    PrintToStderr(output);
  }
};

class StreamBacktraceOutputHandler : public BacktraceOutputHandler {
 public:
  explicit StreamBacktraceOutputHandler(std::ostream* os) : os_(os) {
  }

  StreamBacktraceOutputHandler(const StreamBacktraceOutputHandler&) = delete;
  StreamBacktraceOutputHandler& operator=(const StreamBacktraceOutputHandler&) =
      delete;

  void HandleOutput(const char* output) override { (*os_) << output; }

 private:
  std::ostream* os_;
};

void WarmUpBacktrace() {
  // Warm up stack trace infrastructure. It turns out that on the first
  // call glibc initializes some internal data structures using pthread_once,
  // and even backtrace() can call malloc(), leading to hangs.
  //
  // Example stack trace snippet (with tcmalloc):
  //
  // #8  0x0000000000a173b5 in tc_malloc
  //             at ./third_party/tcmalloc/chromium/src/debugallocation.cc:1161
  // #9  0x00007ffff7de7900 in _dl_map_object_deps at dl-deps.c:517
  // #10 0x00007ffff7ded8a9 in dl_open_worker at dl-open.c:262
  // #11 0x00007ffff7de9176 in _dl_catch_error at dl-error.c:178
  // #12 0x00007ffff7ded31a in _dl_open (file=0x7ffff625e298 "libgcc_s.so.1")
  //             at dl-open.c:639
  // #13 0x00007ffff6215602 in do_dlopen at dl-libc.c:89
  // #14 0x00007ffff7de9176 in _dl_catch_error at dl-error.c:178
  // #15 0x00007ffff62156c4 in dlerror_run at dl-libc.c:48
  // #16 __GI___libc_dlopen_mode at dl-libc.c:165
  // #17 0x00007ffff61ef8f5 in init
  //             at ../sysdeps/x86_64/../ia64/backtrace.c:53
  // #18 0x00007ffff6aad400 in pthread_once
  //             at ../nptl/sysdeps/unix/sysv/linux/x86_64/pthread_once.S:104
  // #19 0x00007ffff61efa14 in __GI___backtrace
  //             at ../sysdeps/x86_64/../ia64/backtrace.c:104
  // #20 0x0000000000752a54 in base::debug::StackTrace::StackTrace
  //             at base/debug/stack_trace_posix.cc:175
  // #21 0x00000000007a4ae5 in
  //             base::(anonymous namespace)::StackDumpSignalHandler
  //             at base/process_util_posix.cc:172
  // #22 <signal handler called>
  StackTrace stack_trace;
}

#if defined(USE_SYMBOLIZE)

// class SandboxSymbolizeHelper.
//
// The purpose of this class is to prepare and install a "file open" callback
// needed by the stack trace symbolization code
// (base/third_party/symbolize/symbolize.h) so that it can function properly
// in a sandboxed process.  The caveat is that this class must be instantiated
// before the sandboxing is enabled so that it can get the chance to open all
// the object files that are loaded in the virtual address space of the current
// process.
class SandboxSymbolizeHelper {
 public:
  // Returns the singleton instance.
  static SandboxSymbolizeHelper* GetInstance() {
    return Singleton<SandboxSymbolizeHelper,
                     LeakySingletonTraits<SandboxSymbolizeHelper>>::get();
  }

  SandboxSymbolizeHelper(const SandboxSymbolizeHelper&) = delete;
  SandboxSymbolizeHelper& operator=(const SandboxSymbolizeHelper&) = delete;

 private:
  friend struct DefaultSingletonTraits<SandboxSymbolizeHelper>;

  SandboxSymbolizeHelper()
      : is_initialized_(false) {
    Init();
  }

  ~SandboxSymbolizeHelper() {
    UnregisterCallback();
    CloseObjectFiles();
  }

  // Returns a O_RDONLY file descriptor for |file_path| if it was opened
  // successfully during the initialization.  The file is repositioned at
  // offset 0.
  // IMPORTANT: This function must be async-signal-safe because it can be
  // called from a signal handler (symbolizing stack frames for a crash).
  int GetFileDescriptor(const char* file_path) {
    int fd = -1;

#if !defined(OFFICIAL_BUILD) || !defined(NO_UNWIND_TABLES)
    if (file_path) {
      // The assumption here is that iterating over std::map<std::string,
      // base::ScopedFD> does not allocate dynamic memory, hence it is
      // async-signal-safe.
      for (const auto& filepath_fd : modules_) {
        if (strcmp(filepath_fd.first.c_str(), file_path) == 0) {
          // POSIX.1-2004 requires an implementation to guarantee that dup()
          // is async-signal-safe.
          fd = HANDLE_EINTR(dup(filepath_fd.second.get()));
          break;
        }
      }
      // POSIX.1-2004 requires an implementation to guarantee that lseek()
      // is async-signal-safe.
      if (fd >= 0 && lseek(fd, 0, SEEK_SET) < 0) {
        // Failed to seek.
        fd = -1;
      }
    }
#endif  // !defined(OFFICIAL_BUILD) || !defined(NO_UNWIND_TABLES)

    return fd;
  }

  // Searches for the object file (from /proc/self/maps) that contains
  // the specified pc.  If found, sets |start_address| to the start address
  // of where this object file is mapped in memory, sets the module base
  // address into |base_address|, copies the object file name into
  // |out_file_name|, and attempts to open the object file.  If the object
  // file is opened successfully, returns the file descriptor.  Otherwise,
  // returns -1.  |out_file_name_size| is the size of the file name buffer
  // (including the null terminator).
  // IMPORTANT: This function must be async-signal-safe because it can be
  // called from a signal handler (symbolizing stack frames for a crash).
  static int OpenObjectFileContainingPc(uint64_t pc, uint64_t& start_address,
                                        uint64_t& base_address, char* file_path,
                                        int file_path_size) {
    // This method can only be called after the singleton is instantiated.
    // This is ensured by the following facts:
    // * This is the only static method in this class, it is private, and
    //   the class has no friends (except for the DefaultSingletonTraits).
    //   The compiler guarantees that it can only be called after the
    //   singleton is instantiated.
    // * This method is used as a callback for the stack tracing code and
    //   the callback registration is done in the constructor, so logically
    //   it cannot be called before the singleton is created.
    SandboxSymbolizeHelper* instance = GetInstance();

    // Cannot use STL iterators here, since debug iterators use locks.
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (size_t i = 0; i < instance->regions_.size(); ++i) {
      const MappedMemoryRegion& region = instance->regions_[i];
      if (region.start <= pc && pc < region.end) {
        start_address = region.start;
        base_address = region.base;
        if (file_path && file_path_size > 0) {
          strncpy(file_path, region.path.c_str(), file_path_size);
          // Ensure null termination.
          file_path[file_path_size - 1] = '\0';
        }
        return instance->GetFileDescriptor(region.path.c_str());
      }
    }
    return -1;
  }

  // Set the base address for each memory region by reading ELF headers in
  // process memory.
  void SetBaseAddressesForMemoryRegions() {
    base::ScopedFD mem_fd(
        HANDLE_EINTR(open("/proc/self/mem", O_RDONLY | O_CLOEXEC)));
    if (!mem_fd.is_valid())
      return;

    auto safe_memcpy = [&mem_fd](void* dst, uintptr_t src, size_t size) {
      return HANDLE_EINTR(pread(mem_fd.get(), dst, size, src)) == ssize_t(size);
    };

    uintptr_t cur_base = 0;
    for (auto& r : regions_) {
      ElfW(Ehdr) ehdr;
      static_assert(SELFMAG <= sizeof(ElfW(Ehdr)), "SELFMAG too large");
      if ((r.permissions & MappedMemoryRegion::READ) &&
          safe_memcpy(&ehdr, r.start, sizeof(ElfW(Ehdr))) &&
          memcmp(ehdr.e_ident, ELFMAG, SELFMAG) == 0) {
        switch (ehdr.e_type) {
          case ET_EXEC:
            cur_base = 0;
            break;
          case ET_DYN:
            // Find the segment containing file offset 0. This will correspond
            // to the ELF header that we just read. Normally this will have
            // virtual address 0, but this is not guaranteed. We must subtract
            // the virtual address from the address where the ELF header was
            // mapped to get the base address.
            //
            // If we fail to find a segment for file offset 0, use the address
            // of the ELF header as the base address.
            cur_base = r.start;
            for (unsigned i = 0; i != ehdr.e_phnum; ++i) {
              ElfW(Phdr) phdr;
              if (safe_memcpy(&phdr, r.start + ehdr.e_phoff + i * sizeof(phdr),
                              sizeof(phdr)) &&
                  phdr.p_type == PT_LOAD && phdr.p_offset == 0) {
                cur_base = r.start - phdr.p_vaddr;
                break;
              }
            }
            break;
          default:
            // ET_REL or ET_CORE. These aren't directly executable, so they
            // don't affect the base address.
            break;
        }
      }

      r.base = cur_base;
    }
  }

  // Parses /proc/self/maps in order to compile a list of all object file names
  // for the modules that are loaded in the current process.
  // Returns true on success.
  bool CacheMemoryRegions() {
    // Reads /proc/self/maps.
    std::string contents;
    if (!ReadProcMaps(&contents)) {
      LOG(ERROR) << "Failed to read /proc/self/maps";
      return false;
    }

    // Parses /proc/self/maps.
    if (!ParseProcMaps(contents, &regions_)) {
      LOG(ERROR) << "Failed to parse the contents of /proc/self/maps";
      return false;
    }

    SetBaseAddressesForMemoryRegions();

    is_initialized_ = true;
    return true;
  }

  // Opens all object files and caches their file descriptors.
  void OpenSymbolFiles() {
    // Pre-opening and caching the file descriptors of all loaded modules is
    // not safe for production builds.  Hence it is only done in non-official
    // builds.  For more details, take a look at: http://crbug.com/341966.
#if !defined(OFFICIAL_BUILD) || !defined(NO_UNWIND_TABLES)
    // Open the object files for all read-only executable regions and cache
    // their file descriptors.
    std::vector<MappedMemoryRegion>::const_iterator it;
    for (it = regions_.begin(); it != regions_.end(); ++it) {
      const MappedMemoryRegion& region = *it;
      // Only interesed in read-only executable regions.
      if ((region.permissions & MappedMemoryRegion::READ) ==
              MappedMemoryRegion::READ &&
          (region.permissions & MappedMemoryRegion::WRITE) == 0 &&
          (region.permissions & MappedMemoryRegion::EXECUTE) ==
              MappedMemoryRegion::EXECUTE) {
        if (region.path.empty()) {
          // Skip regions with empty file names.
          continue;
        }
        if (region.path[0] == '[') {
          // Skip pseudo-paths, like [stack], [vdso], [heap], etc ...
          continue;
        }
        if (base::EndsWith(region.path, " (deleted)",
                           base::CompareCase::SENSITIVE)) {
          // Skip deleted files.
          continue;
        }
        // Avoid duplicates.
        if (modules_.find(region.path) == modules_.end()) {
          int fd = open(region.path.c_str(), O_RDONLY | O_CLOEXEC);
          if (fd >= 0) {
            modules_.emplace(region.path, base::ScopedFD(fd));
          } else {
            LOG(WARNING) << "Failed to open file: " << region.path
                         << "\n  Error: " << strerror(errno);
          }
        }
      }
    }
#endif  // !defined(OFFICIAL_BUILD) || !defined(NO_UNWIND_TABLES)
  }

  // Initializes and installs the symbolization callback.
  void Init() {
    if (CacheMemoryRegions()) {
      OpenSymbolFiles();
      google::InstallSymbolizeOpenObjectFileCallback(
          &OpenObjectFileContainingPc);
    }
  }

  // Unregister symbolization callback.
  void UnregisterCallback() {
    if (is_initialized_) {
      google::InstallSymbolizeOpenObjectFileCallback(nullptr);
      is_initialized_ = false;
    }
  }

  // Closes all file descriptors owned by this instance.
  void CloseObjectFiles() {
#if !defined(OFFICIAL_BUILD) || !defined(NO_UNWIND_TABLES)
    modules_.clear();
#endif  // !defined(OFFICIAL_BUILD) || !defined(NO_UNWIND_TABLES)
  }

  // Set to true upon successful initialization.
  bool is_initialized_;

#if !defined(OFFICIAL_BUILD) || !defined(NO_UNWIND_TABLES)
  // Mapping from file name to file descriptor.  Includes file descriptors
  // for all successfully opened object files and the file descriptor for
  // /proc/self/maps.  This code is not safe for production builds.
  std::map<std::string, base::ScopedFD> modules_;
#endif  // !defined(OFFICIAL_BUILD) || !defined(NO_UNWIND_TABLES)

  // Cache for the process memory regions.  Produced by parsing the contents
  // of /proc/self/maps cache.
  std::vector<MappedMemoryRegion> regions_;
};
#endif  // USE_SYMBOLIZE

}  // namespace

bool EnableInProcessStackDumping() {
#if defined(USE_SYMBOLIZE)
  SandboxSymbolizeHelper::GetInstance();
#endif  // USE_SYMBOLIZE

  // When running in an application, our code typically expects SIGPIPE
  // to be ignored.  Therefore, when testing that same code, it should run
  // with SIGPIPE ignored as well.
  struct sigaction sigpipe_action;
  memset(&sigpipe_action, 0, sizeof(sigpipe_action));
  sigpipe_action.sa_handler = SIG_IGN;
  sigemptyset(&sigpipe_action.sa_mask);
  bool success = (sigaction(SIGPIPE, &sigpipe_action, nullptr) == 0);

  // Avoid hangs during backtrace initialization, see above.
  WarmUpBacktrace();

  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_flags = SA_RESETHAND | SA_SIGINFO;
  action.sa_sigaction = &StackDumpSignalHandler;
  sigemptyset(&action.sa_mask);

  success &= (sigaction(SIGILL, &action, nullptr) == 0);
  success &= (sigaction(SIGABRT, &action, nullptr) == 0);
  success &= (sigaction(SIGFPE, &action, nullptr) == 0);
  success &= (sigaction(SIGBUS, &action, nullptr) == 0);
  success &= (sigaction(SIGSEGV, &action, nullptr) == 0);
// On Linux, SIGSYS is reserved by the kernel for seccomp-bpf sandboxing.
#if !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)
  success &= (sigaction(SIGSYS, &action, nullptr) == 0);
#endif  // !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)

  return success;
}

#if !BUILDFLAG(IS_NACL)
bool SetStackDumpFirstChanceCallback(bool (*handler)(int, siginfo_t*, void*)) {
  DCHECK(try_handle_signal == nullptr || handler == nullptr);
  try_handle_signal = handler;

#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
    defined(THREAD_SANITIZER) || defined(LEAK_SANITIZER) ||    \
    defined(UNDEFINED_SANITIZER)
  struct sigaction installed_handler;
  CHECK_EQ(sigaction(SIGSEGV, NULL, &installed_handler), 0);
  // If the installed handler does not point to StackDumpSignalHandler, then
  // allow_user_segv_handler is 0.
  if (installed_handler.sa_sigaction != StackDumpSignalHandler) {
    LOG(WARNING)
        << "WARNING: sanitizers are preventing signal handler installation. "
        << "WebAssembly trap handlers are disabled.\n";
    return false;
  }
#endif
  return true;
}
#endif

size_t CollectStackTrace(void** trace, size_t count) {
  // NOTE: This code MUST be async-signal safe (it's used by in-process
  // stack dumping signal handler). NO malloc or stdio is allowed here.

#if defined(NO_UNWIND_TABLES) && BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)
  // If we do not have unwind tables, then try tracing using frame pointers.
  return base::debug::TraceStackFramePointers(const_cast<const void**>(trace),
                                              count, 0);
#elif !defined(__UCLIBC__) && !defined(_AIX)
  // Though the backtrace API man page does not list any possible negative
  // return values, we take no chance.
  return base::saturated_cast<size_t>(backtrace(trace, count));
#else
  return 0;
#endif
}

void StackTrace::PrintWithPrefix(const char* prefix_string) const {
// NOTE: This code MUST be async-signal safe (it's used by in-process
// stack dumping signal handler). NO malloc or stdio is allowed here.

#if !defined(__UCLIBC__) && !defined(_AIX)
  PrintBacktraceOutputHandler handler;
  ProcessBacktrace(trace_, count_, prefix_string, &handler);
#endif
}

#if !defined(__UCLIBC__) && !defined(_AIX)
void StackTrace::OutputToStreamWithPrefix(std::ostream* os,
                                          const char* prefix_string) const {
  StreamBacktraceOutputHandler handler(os);
  ProcessBacktrace(trace_, count_, prefix_string, &handler);
}
#endif

namespace internal {

// NOTE: code from sandbox/linux/seccomp-bpf/demo.cc.
char* itoa_r(intptr_t i, char* buf, size_t sz, int base, size_t padding) {
  // Make sure we can write at least one NUL byte.
  size_t n = 1;
  if (n > sz)
    return nullptr;

  if (base < 2 || base > 16) {
    buf[0] = '\000';
    return nullptr;
  }

  char* start = buf;

  uintptr_t j = i;

  // Handle negative numbers (only for base 10).
  if (i < 0 && base == 10) {
    // This does "j = -i" while avoiding integer overflow.
    j = static_cast<uintptr_t>(-(i + 1)) + 1;

    // Make sure we can write the '-' character.
    if (++n > sz) {
      buf[0] = '\000';
      return nullptr;
    }
    *start++ = '-';
  }

  // Loop until we have converted the entire number. Output at least one
  // character (i.e. '0').
  char* ptr = start;
  do {
    // Make sure there is still enough space left in our output buffer.
    if (++n > sz) {
      buf[0] = '\000';
      return nullptr;
    }

    // Output the next digit.
    *ptr++ = "0123456789abcdef"[j % base];
    j /= base;

    if (padding > 0)
      padding--;
  } while (j > 0 || padding > 0);

  // Terminate the output with a NUL character.
  *ptr = '\000';

  // Conversion to ASCII actually resulted in the digits being in reverse
  // order. We can't easily generate them in forward order, as we can't tell
  // the number of characters needed until we are done converting.
  // So, now, we reverse the string (except for the possible "-" sign).
  while (--ptr > start) {
    char ch = *ptr;
    *ptr = *start;
    *start++ = ch;
  }
  return buf;
}

}  // namespace internal

}  // namespace debug
}  // namespace base
