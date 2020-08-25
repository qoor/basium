// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/cpu.h"

#include <inttypes.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <sstream>
#include <utility>

#include "base/stl_util.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID) || \
    defined(OS_AIX)
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/process/internal_linux.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_restrictions.h"
#endif

#if defined(ARCH_CPU_ARM_FAMILY) && \
    (defined(OS_ANDROID) || defined(OS_LINUX) || defined(OS_CHROMEOS))
#include "base/files/file_util.h"
#endif

#if defined(ARCH_CPU_X86_FAMILY)
#if defined(COMPILER_MSVC)
#include <intrin.h>
#include <immintrin.h>  // For _xgetbv()
#endif
#endif

namespace base {

#if defined(ARCH_CPU_X86_FAMILY)
namespace internal {

std::tuple<int, int, int, int> ComputeX86FamilyAndModel(
    const std::string& vendor,
    int signature) {
  int family = (signature >> 8) & 0xf;
  int model = (signature >> 4) & 0xf;
  int ext_family = 0;
  int ext_model = 0;

  // The "Intel 64 and IA-32 Architectures Developer's Manual: Vol. 2A"
  // specifies the Extended Model is defined only when the Base Family is
  // 06h or 0Fh.
  // The "AMD CPUID Specification" specifies that the Extended Model is
  // defined only when Base Family is 0Fh.
  // Both manuals define the display model as
  // {ExtendedModel[3:0],BaseModel[3:0]} in that case.
  if (family == 0xf || (family == 0x6 && vendor == "GenuineIntel")) {
    ext_model = (signature >> 16) & 0xf;
    model += ext_model << 4;
  }
  // Both the "Intel 64 and IA-32 Architectures Developer's Manual: Vol. 2A"
  // and the "AMD CPUID Specification" specify that the Extended Family is
  // defined only when the Base Family is 0Fh.
  // Both manuals define the display family as {0000b,BaseFamily[3:0]} +
  // ExtendedFamily[7:0] in that case.
  if (family == 0xf) {
    ext_family = (signature >> 20) & 0xff;
    family += ext_family;
  }

  return {family, model, ext_family, ext_model};
}

}  // namespace internal
#endif  // defined(ARCH_CPU_X86_FAMILY)

CPU::CPU()
  : signature_(0),
    type_(0),
    family_(0),
    model_(0),
    stepping_(0),
    ext_model_(0),
    ext_family_(0),
    has_mmx_(false),
    has_sse_(false),
    has_sse2_(false),
    has_sse3_(false),
    has_ssse3_(false),
    has_sse41_(false),
    has_sse42_(false),
    has_popcnt_(false),
    has_avx_(false),
    has_avx2_(false),
    has_aesni_(false),
    has_non_stop_time_stamp_counter_(false),
    is_running_in_vm_(false),
    cpu_vendor_("unknown") {
  Initialize();
}

namespace {

#if defined(ARCH_CPU_X86_FAMILY)
#if !defined(COMPILER_MSVC)

#if defined(__pic__) && defined(__i386__)

void __cpuid(int cpu_info[4], int info_type) {
  __asm__ volatile(
      "mov %%ebx, %%edi\n"
      "cpuid\n"
      "xchg %%edi, %%ebx\n"
      : "=a"(cpu_info[0]), "=D"(cpu_info[1]), "=c"(cpu_info[2]),
        "=d"(cpu_info[3])
      : "a"(info_type), "c"(0));
}

#else

void __cpuid(int cpu_info[4], int info_type) {
  __asm__ volatile("cpuid\n"
                   : "=a"(cpu_info[0]), "=b"(cpu_info[1]), "=c"(cpu_info[2]),
                     "=d"(cpu_info[3])
                   : "a"(info_type), "c"(0));
}

#endif
#endif  // !defined(COMPILER_MSVC)

// xgetbv returns the value of an Intel Extended Control Register (XCR).
// Currently only XCR0 is defined by Intel so |xcr| should always be zero.
uint64_t xgetbv(uint32_t xcr) {
#if defined(COMPILER_MSVC)
  return _xgetbv(xcr);
#else
  uint32_t eax, edx;

  __asm__ volatile (
    "xgetbv" : "=a"(eax), "=d"(edx) : "c"(xcr));
  return (static_cast<uint64_t>(edx) << 32) | eax;
#endif  // defined(COMPILER_MSVC)
}

#endif  // ARCH_CPU_X86_FAMILY

#if defined(ARCH_CPU_ARM_FAMILY) && \
    (defined(OS_ANDROID) || defined(OS_LINUX) || defined(OS_CHROMEOS))
std::string* CpuInfoBrand() {
  static std::string* brand = []() {
    // This function finds the value from /proc/cpuinfo under the key "model
    // name" or "Processor". "model name" is used in Linux 3.8 and later (3.7
    // and later for arm64) and is shown once per CPU. "Processor" is used in
    // earler versions and is shown only once at the top of /proc/cpuinfo
    // regardless of the number CPUs.
    const char kModelNamePrefix[] = "model name\t: ";
    const char kProcessorPrefix[] = "Processor\t: ";

    std::string contents;
    ReadFileToString(FilePath("/proc/cpuinfo"), &contents);
    DCHECK(!contents.empty());

    std::istringstream iss(contents);
    std::string line;
    while (std::getline(iss, line)) {
      if (line.compare(0, strlen(kModelNamePrefix), kModelNamePrefix) == 0)
        return new std::string(line.substr(strlen(kModelNamePrefix)));
      if (line.compare(0, strlen(kProcessorPrefix), kProcessorPrefix) == 0)
        return new std::string(line.substr(strlen(kProcessorPrefix)));
    }

    return new std::string();
  }();

  return brand;
}
#endif  // defined(ARCH_CPU_ARM_FAMILY) && (defined(OS_ANDROID) ||
        // defined(OS_LINUX) || defined(OS_CHROMEOS))

}  // namespace

void CPU::Initialize() {
#if defined(ARCH_CPU_X86_FAMILY)
  int cpu_info[4] = {-1};
  // This array is used to temporarily hold the vendor name and then the brand
  // name. Thus it has to be big enough for both use cases. There are
  // static_asserts below for each of the use cases to make sure this array is
  // big enough.
  char cpu_string[sizeof(cpu_info) * 3 + 1];

  // __cpuid with an InfoType argument of 0 returns the number of
  // valid Ids in CPUInfo[0] and the CPU identification string in
  // the other three array elements. The CPU identification string is
  // not in linear order. The code below arranges the information
  // in a human readable form. The human readable order is CPUInfo[1] |
  // CPUInfo[3] | CPUInfo[2]. CPUInfo[2] and CPUInfo[3] are swapped
  // before using memcpy() to copy these three array elements to |cpu_string|.
  __cpuid(cpu_info, 0);
  int num_ids = cpu_info[0];
  std::swap(cpu_info[2], cpu_info[3]);
  static constexpr size_t kVendorNameSize = 3 * sizeof(cpu_info[1]);
  static_assert(kVendorNameSize < base::size(cpu_string),
                "cpu_string too small");
  memcpy(cpu_string, &cpu_info[1], kVendorNameSize);
  cpu_string[kVendorNameSize] = '\0';
  cpu_vendor_ = cpu_string;

  // Interpret CPU feature information.
  if (num_ids > 0) {
    int cpu_info7[4] = {0};
    __cpuid(cpu_info, 1);
    if (num_ids >= 7) {
      __cpuid(cpu_info7, 7);
    }
    signature_ = cpu_info[0];
    stepping_ = cpu_info[0] & 0xf;
    type_ = (cpu_info[0] >> 12) & 0x3;
    std::tie(family_, model_, ext_family_, ext_model_) =
        internal::ComputeX86FamilyAndModel(cpu_vendor_, signature_);
    has_mmx_ =   (cpu_info[3] & 0x00800000) != 0;
    has_sse_ =   (cpu_info[3] & 0x02000000) != 0;
    has_sse2_ =  (cpu_info[3] & 0x04000000) != 0;
    has_sse3_ =  (cpu_info[2] & 0x00000001) != 0;
    has_ssse3_ = (cpu_info[2] & 0x00000200) != 0;
    has_sse41_ = (cpu_info[2] & 0x00080000) != 0;
    has_sse42_ = (cpu_info[2] & 0x00100000) != 0;
    has_popcnt_ = (cpu_info[2] & 0x00800000) != 0;

    // "Hypervisor Present Bit: Bit 31 of ECX of CPUID leaf 0x1."
    // See https://lwn.net/Articles/301888/
    // This is checking for any hypervisor. Hypervisors may choose not to
    // announce themselves. Hypervisors trap CPUID and sometimes return
    // different results to underlying hardware.
    is_running_in_vm_ = (cpu_info[2] & 0x80000000) != 0;

    // AVX instructions will generate an illegal instruction exception unless
    //   a) they are supported by the CPU,
    //   b) XSAVE is supported by the CPU and
    //   c) XSAVE is enabled by the kernel.
    // See http://software.intel.com/en-us/blogs/2011/04/14/is-avx-enabled
    //
    // In addition, we have observed some crashes with the xgetbv instruction
    // even after following Intel's example code. (See crbug.com/375968.)
    // Because of that, we also test the XSAVE bit because its description in
    // the CPUID documentation suggests that it signals xgetbv support.
    has_avx_ =
        (cpu_info[2] & 0x10000000) != 0 &&
        (cpu_info[2] & 0x04000000) != 0 /* XSAVE */ &&
        (cpu_info[2] & 0x08000000) != 0 /* OSXSAVE */ &&
        (xgetbv(0) & 6) == 6 /* XSAVE enabled by kernel */;
    has_aesni_ = (cpu_info[2] & 0x02000000) != 0;
    has_avx2_ = has_avx_ && (cpu_info7[1] & 0x00000020) != 0;
  }

  // Get the brand string of the cpu.
  __cpuid(cpu_info, 0x80000000);
  const int max_parameter = cpu_info[0];

  static constexpr int kParameterStart = 0x80000002;
  static constexpr int kParameterEnd = 0x80000004;
  static constexpr int kParameterSize = kParameterEnd - kParameterStart + 1;
  static_assert(kParameterSize * sizeof(cpu_info) + 1 == base::size(cpu_string),
                "cpu_string has wrong size");

  if (max_parameter >= kParameterEnd) {
    size_t i = 0;
    for (int parameter = kParameterStart; parameter <= kParameterEnd;
         ++parameter) {
      __cpuid(cpu_info, parameter);
      memcpy(&cpu_string[i], cpu_info, sizeof(cpu_info));
      i += sizeof(cpu_info);
    }
    cpu_string[i] = '\0';
    cpu_brand_ = cpu_string;
  }

  static constexpr int kParameterContainingNonStopTimeStampCounter = 0x80000007;
  if (max_parameter >= kParameterContainingNonStopTimeStampCounter) {
    __cpuid(cpu_info, kParameterContainingNonStopTimeStampCounter);
    has_non_stop_time_stamp_counter_ = (cpu_info[3] & (1 << 8)) != 0;
  }

  if (!has_non_stop_time_stamp_counter_ && is_running_in_vm_) {
    int cpu_info_hv[4] = {};
    __cpuid(cpu_info_hv, 0x40000000);
    if (cpu_info_hv[1] == 0x7263694D &&  // Micr
        cpu_info_hv[2] == 0x666F736F &&  // osof
        cpu_info_hv[3] == 0x76482074) {  // t Hv
      // If CPUID says we have a variant TSC and a hypervisor has identified
      // itself and the hypervisor says it is Microsoft Hyper-V, then treat
      // TSC as invariant.
      //
      // Microsoft Hyper-V hypervisor reports variant TSC as there are some
      // scenarios (eg. VM live migration) where the TSC is variant, but for
      // our purposes we can treat it as invariant.
      has_non_stop_time_stamp_counter_ = true;
    }
  }
#elif defined(ARCH_CPU_ARM_FAMILY)
#if defined(OS_ANDROID) || defined(OS_LINUX) || defined(OS_CHROMEOS)
  cpu_brand_ = *CpuInfoBrand();
#elif defined(OS_WIN)
  // Windows makes high-resolution thread timing information available in
  // user-space.
  has_non_stop_time_stamp_counter_ = true;
#endif
#endif
}

CPU::IntelMicroArchitecture CPU::GetIntelMicroArchitecture() const {
  if (has_avx2()) return AVX2;
  if (has_avx()) return AVX;
  if (has_sse42()) return SSE42;
  if (has_sse41()) return SSE41;
  if (has_ssse3()) return SSSE3;
  if (has_sse3()) return SSE3;
  if (has_sse2()) return SSE2;
  if (has_sse()) return SSE;
  return PENTIUM;
}

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID) || \
  defined(OS_AIX)
namespace {

constexpr char kTimeInStatePath[] =
    "/sys/devices/system/cpu/cpu%d/cpufreq/stats/time_in_state";
constexpr char kPhysicalPackageIdPath[] =
    "/sys/devices/system/cpu/cpu%d/topology/physical_package_id";

bool SupportsTimeInState() {
  // Reading from time_in_state doesn't block (it amounts to reading a struct
  // from the cpufreq-stats kernel driver).
  ThreadRestrictions::ScopedAllowIO allow_io;
  // Check if the time_in_state path for the first core is readable.
  FilePath time_in_state_path(StringPrintf(kTimeInStatePath, /*core_index=*/0));
  ScopedFILE file_stream(OpenFile(time_in_state_path, "rb"));
  return static_cast<bool>(file_stream);
}

bool ParseTimeInState(const std::string& content,
                      CPU::CoreType core_type,
                      uint32_t core_index,
                      CPU::TimeInState& time_in_state) {
  const char* begin = content.data();
  size_t max_pos = content.size() - 1;

  // Example time_in_state content:
  // ---
  // 300000 1
  // 403200 0
  // 499200 15
  // ---

  // Iterate over the individual lines.
  for (size_t pos = 0; pos <= max_pos;) {
    int num_chars = 0;

    // Each line should have two integer fields, frequency (kHz) and time (in
    // jiffies), separated by a space, e.g. "2419200 132".
    uint64_t frequency;
    uint64_t time;
    int matches = sscanf(begin + pos, "%" PRIu64 " %" PRIu64 "\n%n", &frequency,
                         &time, &num_chars);
    if (matches != 2)
      return false;

    // Skip zero-valued entries in the output list (no time spent at this
    // frequency).
    if (time > 0) {
      time_in_state.push_back({core_type, core_index, frequency,
                               internal::ClockTicksToTimeDelta(time)});
    }

    // Advance line.
    DCHECK_GT(num_chars, 0);
    pos += num_chars;
  }

  return true;
}

}  // namespace

// static
std::vector<CPU::CoreType> CPU::GuessCoreTypes() {
  // Try to guess the CPU architecture and cores of each cluster by comparing
  // the maximum frequencies of the available (online and offline) cores.
  const char kCPUMaxFreqPath[] =
      "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq";
  int num_cpus = SysInfo::NumberOfProcessors();
  std::vector<CPU::CoreType> core_index_to_type(num_cpus, CoreType::kUnknown);

  std::vector<uint32_t> max_core_frequencies_mhz(num_cpus, 0);
  flat_set<uint32_t> frequencies_mhz;

  {
    // Reading from cpuinfo_max_freq doesn't block (it amounts to reading a
    // struct field from the cpufreq kernel driver).
    ThreadRestrictions::ScopedAllowIO allow_io;
    for (int core_index = 0; core_index < num_cpus; ++core_index) {
      std::string content;
      uint32_t frequency_khz = 0;
      auto path = StringPrintf(kCPUMaxFreqPath, core_index);
      if (ReadFileToString(FilePath(path), &content))
        StringToUint(content, &frequency_khz);
      uint32_t frequency_mhz = frequency_khz / 1000;
      max_core_frequencies_mhz[core_index] = frequency_mhz;
      if (frequency_mhz > 0)
        frequencies_mhz.insert(frequency_mhz);
    }
  }

  size_t num_frequencies = frequencies_mhz.size();

  for (int core_index = 0; core_index < num_cpus; ++core_index) {
    uint32_t core_frequency_mhz = max_core_frequencies_mhz[core_index];

    CoreType core_type = CoreType::kOther;
    if (num_frequencies == 1u) {
      core_type = CoreType::kSymmetric;
    } else if (num_frequencies == 2u || num_frequencies == 3u) {
      auto it = frequencies_mhz.find(core_frequency_mhz);
      if (it != frequencies_mhz.end()) {
        // flat_set is sorted.
        size_t frequency_index = it - frequencies_mhz.begin();
        switch (frequency_index) {
          case 0:
            core_type = num_frequencies == 2u
                            ? CoreType::kBigLittle_Little
                            : CoreType::kBigLittleBigger_Little;
            break;
          case 1:
            core_type = num_frequencies == 2u ? CoreType::kBigLittle_Big
                                              : CoreType::kBigLittleBigger_Big;
            break;
          case 2:
            DCHECK_EQ(num_frequencies, 3u);
            core_type = CoreType::kBigLittleBigger_Bigger;
            break;
          default:
            NOTREACHED();
            break;
        }
      }
    }
    core_index_to_type[core_index] = core_type;
  }

  return core_index_to_type;
}

// static
bool CPU::GetTimeInState(TimeInState& time_in_state) {
  time_in_state.clear();

  // The kernel may not support the cpufreq-stats driver.
  static const bool kSupportsTimeInState = SupportsTimeInState();
  if (!kSupportsTimeInState)
    return false;

  static NoDestructor<std::vector<CoreType>> kCoreTypes(GuessCoreTypes());

  // time_in_state is reported per cluster. Identify the first cores of each
  // cluster.
  static NoDestructor<std::vector<int>> kFirstCoresIndexes([]() {
    std::vector<int> first_cores;
    int last_core_package_id = 0;
    for (int core_index = 0; core_index < SysInfo::NumberOfProcessors();
         core_index++) {
      // Reading from physical_package_id doesn't block (it amounts to reading a
      // struct field from the kernel).
      ThreadRestrictions::ScopedAllowIO allow_io;

      FilePath package_id_path(
          StringPrintf(kPhysicalPackageIdPath, core_index));
      std::string package_id_str;
      if (!ReadFileToString(package_id_path, &package_id_str))
        return std::vector<int>();
      int package_id;
      base::StringPiece trimmed = base::TrimWhitespaceASCII(
          package_id_str, base::TrimPositions::TRIM_ALL);
      if (!base::StringToInt(trimmed, &package_id))
        return std::vector<int>();

      if (last_core_package_id != package_id || core_index == 0)
        first_cores.push_back(core_index);

      last_core_package_id = package_id;
    }
    return first_cores;
  }());

  if (kFirstCoresIndexes->empty())
    return false;

  // Reading from time_in_state doesn't block (it amounts to reading a struct
  // from the cpufreq-stats kernel driver).
  ThreadRestrictions::ScopedAllowIO allow_io;

  // Read the time_in_state for each cluster from the /sys directory of the
  // cluster's first core.
  for (int cluster_core_index : *kFirstCoresIndexes) {
    FilePath time_in_state_path(
        StringPrintf(kTimeInStatePath, cluster_core_index));

    std::string buffer;
    if (!ReadFileToString(time_in_state_path, &buffer))
      return false;

    if (!ParseTimeInState(buffer, (*kCoreTypes)[cluster_core_index],
                          cluster_core_index, time_in_state)) {
      return false;
    }
  }

  return true;
}
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID) ||
        // defined(OS_AIX)

}  // namespace base
