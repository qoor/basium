// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_LOGGING_H_
#define BASE_LOGGING_H_

#include <stddef.h>

#include <cassert>
#include <cstdint>
#include <sstream>
#include <string>

#include "base/base_export.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/dcheck_is_on.h"
#include "base/scoped_clear_last_error.h"
#include "base/strings/string_piece_forward.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include <cstdio>
#endif

//
// Optional message capabilities
// -----------------------------
// Assertion failed messages and fatal errors are displayed in a dialog box
// before the application exits. However, running this UI creates a message
// loop, which causes application messages to be processed and potentially
// dispatched to existing application windows. Since the application is in a
// bad state when this assertion dialog is displayed, these messages may not
// get processed and hang the dialog, or the application might go crazy.
//
// Therefore, it can be beneficial to display the error dialog in a separate
// process from the main application. When the logging system needs to display
// a fatal error dialog box, it will look for a program called
// "DebugMessage.exe" in the same directory as the application executable. It
// will run this application with the message as the command line, and will
// not include the name of the application as is traditional for easier
// parsing.
//
// The code for DebugMessage.exe is only one line. In WinMain, do:
//   MessageBox(NULL, GetCommandLineW(), L"Fatal Error", 0);
//
// If DebugMessage.exe is not found, the logging code will use a normal
// MessageBox, potentially causing the problems discussed above.

// Instructions
// ------------
//
// Make a bunch of macros for logging.  The way to log things is to stream
// things to LOG(<a particular severity level>).  E.g.,
//
//   LOG(INFO) << "Found " << num_cookies << " cookies";
//
// You can also do conditional logging:
//
//   LOG_IF(INFO, num_cookies > 10) << "Got lots of cookies";
//
// The CHECK(condition) macro is active in both debug and release builds and
// effectively performs a LOG(FATAL) which terminates the process and
// generates a crashdump unless a debugger is attached.
//
// There are also "debug mode" logging macros like the ones above:
//
//   DLOG(INFO) << "Found cookies";
//
//   DLOG_IF(INFO, num_cookies > 10) << "Got lots of cookies";
//
// All "debug mode" logging is compiled away to nothing for non-debug mode
// compiles.  LOG_IF and development flags also work well together
// because the code can be compiled away sometimes.
//
// We also have
//
//   LOG_ASSERT(assertion);
//   DLOG_ASSERT(assertion);
//
// which is syntactic sugar for {,D}LOG_IF(FATAL, assert fails) << assertion;
//
// There are "verbose level" logging macros.  They look like
//
//   VLOG(1) << "I'm printed when you run the program with --v=1 or more";
//   VLOG(2) << "I'm printed when you run the program with --v=2 or more";
//
// These always log at the INFO log level (when they log at all).
// The verbose logging can also be turned on module-by-module.  For instance,
//    --vmodule=profile=2,icon_loader=1,browser_*=3,*/chromeos/*=4 --v=0
// will cause:
//   a. VLOG(2) and lower messages to be printed from profile.{h,cc}
//   b. VLOG(1) and lower messages to be printed from icon_loader.{h,cc}
//   c. VLOG(3) and lower messages to be printed from files prefixed with
//      "browser"
//   d. VLOG(4) and lower messages to be printed from files under a
//     "chromeos" directory.
//   e. VLOG(0) and lower messages to be printed from elsewhere
//
// The wildcarding functionality shown by (c) supports both '*' (match
// 0 or more characters) and '?' (match any single character)
// wildcards.  Any pattern containing a forward or backward slash will
// be tested against the whole pathname and not just the module.
// E.g., "*/foo/bar/*=2" would change the logging level for all code
// in source files under a "foo/bar" directory.
//
// Note that for a Chromium binary built in release mode (is_debug = false) you
// must pass "--enable-logging=stderr" in order to see the output of VLOG
// statements.
//
// There's also VLOG_IS_ON(n) "verbose level" condition macro. To be used as
//
//   if (VLOG_IS_ON(2)) {
//     // do some logging preparation and logging
//     // that can't be accomplished with just VLOG(2) << ...;
//   }
//
// There is also a VLOG_IF "verbose level" condition macro for sample
// cases, when some extra computation and preparation for logs is not
// needed.
//
//   VLOG_IF(1, (size > 1024))
//      << "I'm printed when size is more than 1024 and when you run the "
//         "program with --v=1 or more";
//
// We also override the standard 'assert' to use 'DLOG_ASSERT'.
//
// Lastly, there is:
//
//   PLOG(ERROR) << "Couldn't do foo";
//   DPLOG(ERROR) << "Couldn't do foo";
//   PLOG_IF(ERROR, cond) << "Couldn't do foo";
//   DPLOG_IF(ERROR, cond) << "Couldn't do foo";
//   PCHECK(condition) << "Couldn't do foo";
//   DPCHECK(condition) << "Couldn't do foo";
//
// which append the last system error to the message in string form (taken from
// GetLastError() on Windows and errno on POSIX).
//
// The supported severity levels for macros that allow you to specify one
// are (in increasing order of severity) INFO, WARNING, ERROR, and FATAL.
//
// Very important: logging a message at the FATAL severity level causes
// the program to terminate (after the message is logged).
//
// There is the special severity of DFATAL, which logs FATAL in DCHECK-enabled
// builds, ERROR in normal mode.
//
// Output is formatted as per the following example, except on Chrome OS.
// [3816:3877:0812/234555.406952:VERBOSE1:drm_device_handle.cc(90)] Succeeded
// authenticating /dev/dri/card0 in 0 ms with 1 attempt(s)
//
// The colon separated fields inside the brackets are as follows:
// 0. An optional Logfile prefix (not included in this example)
// 1. Process ID
// 2. Thread ID
// 3. The date/time of the log message, in MMDD/HHMMSS.Milliseconds format
// 4. The log level
// 5. The filename and line number where the log was instantiated
//
// Output for Chrome OS can be switched to syslog-like format. See
// InitWithSyslogPrefix() in logging_chromeos.h for details.
//
// Note that the visibility can be changed by setting preferences in
// SetLogItems()
//
// Additional logging-related information can be found here:
// https://chromium.googlesource.com/chromium/src/+/main/docs/linux/debugging.md#Logging

namespace logging {

// TODO(avi): do we want to do a unification of character types here?
#if defined(OS_WIN)
typedef wchar_t PathChar;
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
typedef char PathChar;
#endif

// A bitmask of potential logging destinations.
using LoggingDestination = uint32_t;
// Specifies where logs will be written. Multiple destinations can be specified
// with bitwise OR.
// Unless destination is LOG_NONE, all logs with severity ERROR and above will
// be written to stderr in addition to the specified destination.
enum : uint32_t {
  LOG_NONE                = 0,
  LOG_TO_FILE             = 1 << 0,
  LOG_TO_SYSTEM_DEBUG_LOG = 1 << 1,
  LOG_TO_STDERR           = 1 << 2,

  LOG_TO_ALL = LOG_TO_FILE | LOG_TO_SYSTEM_DEBUG_LOG | LOG_TO_STDERR,

// On Windows, use a file next to the exe.
// On POSIX platforms, where it may not even be possible to locate the
// executable on disk, use stderr.
// On Fuchsia, use the Fuchsia logging service.
#if defined(OS_FUCHSIA) || defined(OS_NACL)
  LOG_DEFAULT = LOG_TO_SYSTEM_DEBUG_LOG,
#elif defined(OS_WIN)
  LOG_DEFAULT = LOG_TO_FILE,
#elif defined(OS_POSIX)
  LOG_DEFAULT = LOG_TO_SYSTEM_DEBUG_LOG | LOG_TO_STDERR,
#endif
};

// Indicates that the log file should be locked when being written to.
// Unless there is only one single-threaded process that is logging to
// the log file, the file should be locked during writes to make each
// log output atomic. Other writers will block.
//
// All processes writing to the log file must have their locking set for it to
// work properly. Defaults to LOCK_LOG_FILE.
enum LogLockingState { LOCK_LOG_FILE, DONT_LOCK_LOG_FILE };

// On startup, should we delete or append to an existing log file (if any)?
// Defaults to APPEND_TO_OLD_LOG_FILE.
enum OldFileDeletionState { DELETE_OLD_LOG_FILE, APPEND_TO_OLD_LOG_FILE };

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Defines the log message prefix format to use.
// LOG_FORMAT_SYSLOG indicates syslog-like message prefixes.
// LOG_FORMAT_CHROME indicates the normal Chrome format.
enum class BASE_EXPORT LogFormat { LOG_FORMAT_CHROME, LOG_FORMAT_SYSLOG };
#endif

struct BASE_EXPORT LoggingSettings {
  // Equivalent to logging destination enum, but allows for multiple
  // destinations.
  uint32_t logging_dest = LOG_DEFAULT;

  // The four settings below have an effect only when LOG_TO_FILE is
  // set in |logging_dest|.
  const PathChar* log_file_path = nullptr;
  LogLockingState lock_log = LOCK_LOG_FILE;
  OldFileDeletionState delete_old = APPEND_TO_OLD_LOG_FILE;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Contains an optional file that logs should be written to. If present,
  // |log_file_path| will be ignored, and the logging system will take ownership
  // of the FILE. If there's an error writing to this file, no fallback paths
  // will be opened.
  FILE* log_file = nullptr;
  // ChromeOS uses the syslog log format by default.
  LogFormat log_format = LogFormat::LOG_FORMAT_SYSLOG;
#endif
};

// Define different names for the BaseInitLoggingImpl() function depending on
// whether NDEBUG is defined or not so that we'll fail to link if someone tries
// to compile logging.cc with NDEBUG but includes logging.h without defining it,
// or vice versa.
#if defined(NDEBUG)
#define BaseInitLoggingImpl BaseInitLoggingImpl_built_with_NDEBUG
#else
#define BaseInitLoggingImpl BaseInitLoggingImpl_built_without_NDEBUG
#endif

// Implementation of the InitLogging() method declared below.  We use a
// more-specific name so we can #define it above without affecting other code
// that has named stuff "InitLogging".
BASE_EXPORT bool BaseInitLoggingImpl(const LoggingSettings& settings);

// Sets the log file name and other global logging state. Calling this function
// is recommended, and is normally done at the beginning of application init.
// If you don't call it, all the flags will be initialized to their default
// values, and there is a race condition that may leak a critical section
// object if two threads try to do the first log at the same time.
// See the definition of the enums above for descriptions and default values.
//
// The default log file is initialized to "debug.log" in the application
// directory. You probably don't want this, especially since the program
// directory may not be writable on an enduser's system.
//
// This function may be called a second time to re-direct logging (e.g after
// loging in to a user partition), however it should never be called more than
// twice.
inline bool InitLogging(const LoggingSettings& settings) {
  return BaseInitLoggingImpl(settings);
}

// Sets the log level. Anything at or above this level will be written to the
// log file/displayed to the user (if applicable). Anything below this level
// will be silently ignored. The log level defaults to 0 (everything is logged
// up to level INFO) if this function is not called.
// Note that log messages for VLOG(x) are logged at level -x, so setting
// the min log level to negative values enables verbose logging.
BASE_EXPORT void SetMinLogLevel(int level);

// Gets the current log level.
BASE_EXPORT int GetMinLogLevel();

// Used by LOG_IS_ON to lazy-evaluate stream arguments.
BASE_EXPORT bool ShouldCreateLogMessage(int severity);

// Gets the VLOG default verbosity level.
BASE_EXPORT int GetVlogVerbosity();

// Note that |N| is the size *with* the null terminator.
BASE_EXPORT int GetVlogLevelHelper(const char* file_start, size_t N);

// Gets the current vlog level for the given file (usually taken from __FILE__).
template <size_t N>
int GetVlogLevel(const char (&file)[N]) {
  return GetVlogLevelHelper(file, N);
}

// Sets the common items you want to be prepended to each log message.
// process and thread IDs default to off, the timestamp defaults to on.
// If this function is not called, logging defaults to writing the timestamp
// only.
BASE_EXPORT void SetLogItems(bool enable_process_id, bool enable_thread_id,
                             bool enable_timestamp, bool enable_tickcount);

// Sets an optional prefix to add to each log message. |prefix| is not copied
// and should be a raw string constant. |prefix| must only contain ASCII letters
// to avoid confusion with PIDs and timestamps. Pass null to remove the prefix.
// Logging defaults to no prefix.
BASE_EXPORT void SetLogPrefix(const char* prefix);

// Sets whether or not you'd like to see fatal debug messages popped up in
// a dialog box or not.
// Dialogs are not shown by default.
BASE_EXPORT void SetShowErrorDialogs(bool enable_dialogs);

// Sets the Log Assert Handler that will be used to notify of check failures.
// Resets Log Assert Handler on object destruction.
// The default handler shows a dialog box and then terminate the process,
// however clients can use this function to override with their own handling
// (e.g. a silent one for Unit Tests)
using LogAssertHandlerFunction =
    base::RepeatingCallback<void(const char* file,
                                 int line,
                                 const base::StringPiece message,
                                 const base::StringPiece stack_trace)>;

class BASE_EXPORT ScopedLogAssertHandler {
 public:
  explicit ScopedLogAssertHandler(LogAssertHandlerFunction handler);
  ScopedLogAssertHandler(const ScopedLogAssertHandler&) = delete;
  ScopedLogAssertHandler& operator=(const ScopedLogAssertHandler&) = delete;
  ~ScopedLogAssertHandler();
};

// Sets the Log Message Handler that gets passed every log message before
// it's sent to other log destinations (if any).
// Returns true to signal that it handled the message and the message
// should not be sent to other log destinations.
typedef bool (*LogMessageHandlerFunction)(int severity,
    const char* file, int line, size_t message_start, const std::string& str);
BASE_EXPORT void SetLogMessageHandler(LogMessageHandlerFunction handler);
BASE_EXPORT LogMessageHandlerFunction GetLogMessageHandler();

using LogSeverity = int;
constexpr LogSeverity LOGGING_VERBOSE = -1;  // This is level 1 verbosity
// Note: the log severities are used to index into the array of names,
// see log_severity_names.
constexpr LogSeverity LOGGING_INFO = 0;
constexpr LogSeverity LOGGING_WARNING = 1;
constexpr LogSeverity LOGGING_ERROR = 2;
constexpr LogSeverity LOGGING_FATAL = 3;
constexpr LogSeverity LOGGING_NUM_SEVERITIES = 4;

// LOGGING_DFATAL is LOGGING_FATAL in DCHECK-enabled builds, ERROR in normal
// mode.
#if DCHECK_IS_ON()
constexpr LogSeverity LOGGING_DFATAL = LOGGING_FATAL;
#else
constexpr LogSeverity LOGGING_DFATAL = LOGGING_ERROR;
#endif

// This block duplicates the above entries to facilitate incremental conversion
// from LOG_FOO to LOGGING_FOO.
// TODO(thestig): Convert existing users to LOGGING_FOO and remove this block.
constexpr LogSeverity LOG_VERBOSE = LOGGING_VERBOSE;
constexpr LogSeverity LOG_INFO = LOGGING_INFO;
constexpr LogSeverity LOG_WARNING = LOGGING_WARNING;
constexpr LogSeverity LOG_ERROR = LOGGING_ERROR;
constexpr LogSeverity LOG_FATAL = LOGGING_FATAL;
constexpr LogSeverity LOG_DFATAL = LOGGING_DFATAL;

// A few definitions of macros that don't generate much code. These are used
// by LOG() and LOG_IF, etc. Since these are used all over our code, it's
// better to have compact code for these operations.
#define COMPACT_GOOGLE_LOG_EX_INFO(ClassName, ...)                  \
  ::logging::ClassName(__FILE__, __LINE__, ::logging::LOGGING_INFO, \
                       ##__VA_ARGS__)
#define COMPACT_GOOGLE_LOG_EX_WARNING(ClassName, ...)                  \
  ::logging::ClassName(__FILE__, __LINE__, ::logging::LOGGING_WARNING, \
                       ##__VA_ARGS__)
#define COMPACT_GOOGLE_LOG_EX_ERROR(ClassName, ...)                  \
  ::logging::ClassName(__FILE__, __LINE__, ::logging::LOGGING_ERROR, \
                       ##__VA_ARGS__)
#define COMPACT_GOOGLE_LOG_EX_FATAL(ClassName, ...)                  \
  ::logging::ClassName(__FILE__, __LINE__, ::logging::LOGGING_FATAL, \
                       ##__VA_ARGS__)
#define COMPACT_GOOGLE_LOG_EX_DFATAL(ClassName, ...)                  \
  ::logging::ClassName(__FILE__, __LINE__, ::logging::LOGGING_DFATAL, \
                       ##__VA_ARGS__)
#define COMPACT_GOOGLE_LOG_EX_DCHECK(ClassName, ...)                  \
  ::logging::ClassName(__FILE__, __LINE__, ::logging::LOGGING_DCHECK, \
                       ##__VA_ARGS__)

#define COMPACT_GOOGLE_LOG_INFO COMPACT_GOOGLE_LOG_EX_INFO(LogMessage)
#define COMPACT_GOOGLE_LOG_WARNING COMPACT_GOOGLE_LOG_EX_WARNING(LogMessage)
#define COMPACT_GOOGLE_LOG_ERROR COMPACT_GOOGLE_LOG_EX_ERROR(LogMessage)
#define COMPACT_GOOGLE_LOG_FATAL COMPACT_GOOGLE_LOG_EX_FATAL(LogMessage)
#define COMPACT_GOOGLE_LOG_DFATAL COMPACT_GOOGLE_LOG_EX_DFATAL(LogMessage)
#define COMPACT_GOOGLE_LOG_DCHECK COMPACT_GOOGLE_LOG_EX_DCHECK(LogMessage)

#if defined(OS_WIN)
// wingdi.h defines ERROR to be 0. When we call LOG(ERROR), it gets
// substituted with 0, and it expands to COMPACT_GOOGLE_LOG_0. To allow us
// to keep using this syntax, we define this macro to do the same thing
// as COMPACT_GOOGLE_LOG_ERROR, and also define ERROR the same way that
// the Windows SDK does for consistency.
#define ERROR 0
#define COMPACT_GOOGLE_LOG_EX_0(ClassName, ...) \
  COMPACT_GOOGLE_LOG_EX_ERROR(ClassName , ##__VA_ARGS__)
#define COMPACT_GOOGLE_LOG_0 COMPACT_GOOGLE_LOG_ERROR
// Needed for LOG_IS_ON(ERROR).
constexpr LogSeverity LOGGING_0 = LOGGING_ERROR;
#endif

// As special cases, we can assume that LOG_IS_ON(FATAL) always holds. Also,
// LOG_IS_ON(DFATAL) always holds in debug mode. In particular, CHECK()s will
// always fire if they fail.
#define LOG_IS_ON(severity) \
  (::logging::ShouldCreateLogMessage(::logging::LOGGING_##severity))

// We don't do any caching tricks with VLOG_IS_ON() like the
// google-glog version since it increases binary size.  This means
// that using the v-logging functions in conjunction with --vmodule
// may be slow.
#define VLOG_IS_ON(verboselevel) \
  ((verboselevel) <= ::logging::GetVlogLevel(__FILE__))

// Helper macro which avoids evaluating the arguments to a stream if
// the condition doesn't hold. Condition is evaluated once and only once.
#define LAZY_STREAM(stream, condition)                                  \
  !(condition) ? (void) 0 : ::logging::LogMessageVoidify() & (stream)

// We use the preprocessor's merging operator, "##", so that, e.g.,
// LOG(INFO) becomes the token COMPACT_GOOGLE_LOG_INFO.  There's some funny
// subtle difference between ostream member streaming functions (e.g.,
// ostream::operator<<(int) and ostream non-member streaming functions
// (e.g., ::operator<<(ostream&, string&): it turns out that it's
// impossible to stream something like a string directly to an unnamed
// ostream. We employ a neat hack by calling the stream() member
// function of LogMessage which seems to avoid the problem.
#define LOG_STREAM(severity) COMPACT_GOOGLE_LOG_ ## severity.stream()

#define LOG(severity) LAZY_STREAM(LOG_STREAM(severity), LOG_IS_ON(severity))
#define LOG_IF(severity, condition) \
  LAZY_STREAM(LOG_STREAM(severity), LOG_IS_ON(severity) && (condition))

// The VLOG macros log with negative verbosities.
#define VLOG_STREAM(verbose_level) \
  ::logging::LogMessage(__FILE__, __LINE__, -(verbose_level)).stream()

#define VLOG(verbose_level) \
  LAZY_STREAM(VLOG_STREAM(verbose_level), VLOG_IS_ON(verbose_level))

#define VLOG_IF(verbose_level, condition) \
  LAZY_STREAM(VLOG_STREAM(verbose_level), \
      VLOG_IS_ON(verbose_level) && (condition))

#if defined (OS_WIN)
#define VPLOG_STREAM(verbose_level) \
  ::logging::Win32ErrorLogMessage(__FILE__, __LINE__, -(verbose_level), \
    ::logging::GetLastSystemErrorCode()).stream()
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
#define VPLOG_STREAM(verbose_level) \
  ::logging::ErrnoLogMessage(__FILE__, __LINE__, -(verbose_level), \
    ::logging::GetLastSystemErrorCode()).stream()
#endif

#define VPLOG(verbose_level) \
  LAZY_STREAM(VPLOG_STREAM(verbose_level), VLOG_IS_ON(verbose_level))

#define VPLOG_IF(verbose_level, condition) \
  LAZY_STREAM(VPLOG_STREAM(verbose_level), \
    VLOG_IS_ON(verbose_level) && (condition))

// TODO(akalin): Add more VLOG variants, e.g. VPLOG.

#define LOG_ASSERT(condition)                       \
  LOG_IF(FATAL, !(ANALYZER_ASSUME_TRUE(condition))) \
      << "Assert failed: " #condition ". "

#if defined(OS_WIN)
#define PLOG_STREAM(severity) \
  COMPACT_GOOGLE_LOG_EX_ ## severity(Win32ErrorLogMessage, \
      ::logging::GetLastSystemErrorCode()).stream()
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
#define PLOG_STREAM(severity) \
  COMPACT_GOOGLE_LOG_EX_ ## severity(ErrnoLogMessage, \
      ::logging::GetLastSystemErrorCode()).stream()
#endif

#define PLOG(severity)                                          \
  LAZY_STREAM(PLOG_STREAM(severity), LOG_IS_ON(severity))

#define PLOG_IF(severity, condition) \
  LAZY_STREAM(PLOG_STREAM(severity), LOG_IS_ON(severity) && (condition))

BASE_EXPORT extern std::ostream* g_swallow_stream;

// Note that g_swallow_stream is used instead of an arbitrary LOG() stream to
// avoid the creation of an object with a non-trivial destructor (LogMessage).
// On MSVC x86 (checked on 2015 Update 3), this causes a few additional
// pointless instructions to be emitted even at full optimization level, even
// though the : arm of the ternary operator is clearly never executed. Using a
// simpler object to be &'d with Voidify() avoids these extra instructions.
// Using a simpler POD object with a templated operator<< also works to avoid
// these instructions. However, this causes warnings on statically defined
// implementations of operator<<(std::ostream, ...) in some .cc files, because
// they become defined-but-unreferenced functions. A reinterpret_cast of 0 to an
// ostream* also is not suitable, because some compilers warn of undefined
// behavior.
#define EAT_STREAM_PARAMETERS \
  true ? (void)0              \
       : ::logging::LogMessageVoidify() & (*::logging::g_swallow_stream)

// Definitions for DLOG et al.

#if DCHECK_IS_ON()

#define DLOG_IS_ON(severity) LOG_IS_ON(severity)
#define DLOG_IF(severity, condition) LOG_IF(severity, condition)
#define DLOG_ASSERT(condition) LOG_ASSERT(condition)
#define DPLOG_IF(severity, condition) PLOG_IF(severity, condition)
#define DVLOG_IF(verboselevel, condition) VLOG_IF(verboselevel, condition)
#define DVPLOG_IF(verboselevel, condition) VPLOG_IF(verboselevel, condition)

#else  // DCHECK_IS_ON()

// If !DCHECK_IS_ON(), we want to avoid emitting any references to |condition|
// (which may reference a variable defined only if DCHECK_IS_ON()).
// Contrast this with DCHECK et al., which has different behavior.

#define DLOG_IS_ON(severity) false
#define DLOG_IF(severity, condition) EAT_STREAM_PARAMETERS
#define DLOG_ASSERT(condition) EAT_STREAM_PARAMETERS
#define DPLOG_IF(severity, condition) EAT_STREAM_PARAMETERS
#define DVLOG_IF(verboselevel, condition) EAT_STREAM_PARAMETERS
#define DVPLOG_IF(verboselevel, condition) EAT_STREAM_PARAMETERS

#endif  // DCHECK_IS_ON()

#define DLOG(severity)                                          \
  LAZY_STREAM(LOG_STREAM(severity), DLOG_IS_ON(severity))

#define DPLOG(severity)                                         \
  LAZY_STREAM(PLOG_STREAM(severity), DLOG_IS_ON(severity))

#define DVLOG(verboselevel) DVLOG_IF(verboselevel, true)

#define DVPLOG(verboselevel) DVPLOG_IF(verboselevel, true)

// Definitions for DCHECK et al.

#if defined(DCHECK_IS_CONFIGURABLE)
BASE_EXPORT extern LogSeverity LOGGING_DCHECK;
#else
constexpr LogSeverity LOGGING_DCHECK = LOGGING_FATAL;
#endif  // defined(DCHECK_IS_CONFIGURABLE)

// Redefine the standard assert to use our nice log files
#undef assert
#define assert(x) DLOG_ASSERT(x)

// This class more or less represents a particular log message.  You
// create an instance of LogMessage and then stream stuff to it.
// When you finish streaming to it, ~LogMessage is called and the
// full message gets streamed to the appropriate destination.
//
// You shouldn't actually use LogMessage's constructor to log things,
// though.  You should use the LOG() macro (and variants thereof)
// above.
class BASE_EXPORT LogMessage {
 public:
  // Used for LOG(severity).
  LogMessage(const char* file, int line, LogSeverity severity);

  // Used for CHECK().  Implied severity = LOGGING_FATAL.
  LogMessage(const char* file, int line, const char* condition);
  LogMessage(const LogMessage&) = delete;
  LogMessage& operator=(const LogMessage&) = delete;
  virtual ~LogMessage();

  std::ostream& stream() { return stream_; }

  LogSeverity severity() { return severity_; }
  std::string str() { return stream_.str(); }

 private:
  void Init(const char* file, int line);

  const LogSeverity severity_;
  std::ostringstream stream_;
  size_t message_start_;  // Offset of the start of the message (past prefix
                          // info).
  // The file and line information passed in to the constructor.
  const char* const file_;
  const int line_;

  // This is useful since the LogMessage class uses a lot of Win32 calls
  // that will lose the value of GLE and the code that called the log function
  // will have lost the thread error value when the log call returns.
  base::ScopedClearLastError last_error_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void InitWithSyslogPrefix(base::StringPiece filename,
                            int line,
                            uint64_t tick_count,
                            const char* log_severity_name_c_str,
                            const char* log_prefix,
                            bool enable_process_id,
                            bool enable_thread_id,
                            bool enable_timestamp,
                            bool enable_tickcount);
#endif
};

// This class is used to explicitly ignore values in the conditional
// logging macros.  This avoids compiler warnings like "value computed
// is not used" and "statement has no effect".
class LogMessageVoidify {
 public:
  LogMessageVoidify() = default;
  // This has to be an operator with a precedence lower than << but
  // higher than ?:
  void operator&(std::ostream&) { }
};

#if defined(OS_WIN)
typedef unsigned long SystemErrorCode;
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
typedef int SystemErrorCode;
#endif

// Alias for ::GetLastError() on Windows and errno on POSIX. Avoids having to
// pull in windows.h just for GetLastError() and DWORD.
BASE_EXPORT SystemErrorCode GetLastSystemErrorCode();
BASE_EXPORT std::string SystemErrorCodeToString(SystemErrorCode error_code);

#if defined(OS_WIN)
// Appends a formatted system message of the GetLastError() type.
class BASE_EXPORT Win32ErrorLogMessage : public LogMessage {
 public:
  Win32ErrorLogMessage(const char* file,
                       int line,
                       LogSeverity severity,
                       SystemErrorCode err);
  Win32ErrorLogMessage(const Win32ErrorLogMessage&) = delete;
  Win32ErrorLogMessage& operator=(const Win32ErrorLogMessage&) = delete;
  // Appends the error message before destructing the encapsulated class.
  ~Win32ErrorLogMessage() override;

 private:
  SystemErrorCode err_;
};
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
// Appends a formatted system message of the errno type
class BASE_EXPORT ErrnoLogMessage : public LogMessage {
 public:
  ErrnoLogMessage(const char* file,
                  int line,
                  LogSeverity severity,
                  SystemErrorCode err);
  ErrnoLogMessage(const ErrnoLogMessage&) = delete;
  ErrnoLogMessage& operator=(const ErrnoLogMessage&) = delete;
  // Appends the error message before destructing the encapsulated class.
  ~ErrnoLogMessage() override;

 private:
  SystemErrorCode err_;
};
#endif  // OS_WIN

// Closes the log file explicitly if open.
// NOTE: Since the log file is opened as necessary by the action of logging
//       statements, there's no guarantee that it will stay closed
//       after this call.
BASE_EXPORT void CloseLogFile();

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Returns a new file handle that will write to the same destination as the
// currently open log file. Returns nullptr if logging to a file is disabled,
// or if opening the file failed. This is intended to be used to initialize
// logging in child processes that are unable to open files.
BASE_EXPORT FILE* DuplicateLogFILE();
#endif

// Async signal safe logging mechanism.
BASE_EXPORT void RawLog(int level, const char* message);

#define RAW_LOG(level, message) \
  ::logging::RawLog(::logging::LOGGING_##level, message)

#if defined(OS_WIN)
// Returns true if logging to file is enabled.
BASE_EXPORT bool IsLoggingToFileEnabled();

// Returns the default log file path.
BASE_EXPORT std::wstring GetLogFileFullPath();
#endif

}  // namespace logging

// Note that "The behavior of a C++ program is undefined if it adds declarations
// or definitions to namespace std or to a namespace within namespace std unless
// otherwise specified." --C++11[namespace.std]
//
// We've checked that this particular definition has the intended behavior on
// our implementations, but it's prone to breaking in the future, and please
// don't imitate this in your own definitions without checking with some
// standard library experts.
namespace std {
// These functions are provided as a convenience for logging, which is where we
// use streams (it is against Google style to use streams in other places). It
// is designed to allow you to emit non-ASCII Unicode strings to the log file,
// which is normally ASCII. It is relatively slow, so try not to use it for
// common cases. Non-ASCII characters will be converted to UTF-8 by these
// operators.
BASE_EXPORT std::ostream& operator<<(std::ostream& out, const wchar_t* wstr);
BASE_EXPORT std::ostream& operator<<(std::ostream& out,
                                     const std::wstring& wstr);

BASE_EXPORT std::ostream& operator<<(std::ostream& out, const char16_t* str16);
BASE_EXPORT std::ostream& operator<<(std::ostream& out,
                                     const std::u16string& str16);
}  // namespace std

#endif  // BASE_LOGGING_H_
