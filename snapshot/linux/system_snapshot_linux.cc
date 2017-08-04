// Copyright 2017 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "snapshot/linux/system_snapshot_linux.h"

#include <stddef.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/utsname.h>

#include <algorithm>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "snapshot/cpu_context.h"
#include "snapshot/posix/timezone.h"
#include "util/file/file_io.h"
#include "util/string/split_string.h"

#if defined(OS_ANDROID)
#include <sys/system_properties.h>
#endif

namespace crashpad {
namespace internal {

namespace {

int getcpu() {
  int cpu, res;
  res = syscall(SYS_getcpu, &cpu, nullptr, nullptr);
  if (res == -1) {
    PLOG(ERROR) << "getcpu";
    return -1;
  }
  return cpu;
}

bool ReadFreqFile(const std::string& filename, uint64_t* hz) {
  std::string contents;
  if (!LoggingReadEntireFile(base::FilePath(filename), &contents)) {
    return false;
  }
  if (contents.back() != '\n') {
    LOG(ERROR) << "format error";
    return false;
  }
  contents.pop_back();

  uint64_t khz;
  if (!base::StringToUint64(base::StringPiece(contents), &khz)) {
    LOG(ERROR) << "format error";
    return false;
  }

  *hz = khz * 1000;
  return true;
}

}  // namespace

SystemSnapshotLinux::SystemSnapshotLinux()
    : SystemSnapshot(),
      os_version_full_(),
      os_version_build_(),
      process_reader_(nullptr),
      snapshot_time_(nullptr),
#if defined(ARCH_CPU_X86_FAMILY)
      cpuid_(),
#endif  // ARCH_CPU_X86_FAMILY
      os_version_major_(-1),
      os_version_minor_(-1),
      os_version_bugfix_(-1),
      initialized_() {
}

SystemSnapshotLinux::~SystemSnapshotLinux() {}

void SystemSnapshotLinux::Initialize(ProcessReader* process_reader,
                                     const timeval* snapshot_time) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  process_reader_ = process_reader;
  snapshot_time_ = snapshot_time;

  utsname uts;
  if (uname(&uts) != 0) {
    PLOG(WARNING) << "uname";
  } else {
    os_version_full_ = base::StringPrintf(
        "%s %s %s %s", uts.sysname, uts.release, uts.version, uts.machine);
  }
  ReadKernelVersion(uts.release);

  os_version_build_.push_back(' ');
  os_version_build_ += uts.version;
  os_version_build_.push_back(' ');
  os_version_build_ += uts.machine;

#if defined(OS_ANDROID)
  char build_string[PROP_VALUE_MAX];
  int length = __system_property_get("ro.build.fingerprint", build_string);
  if (length <= 0) {
    LOG(WARNING) << "Couldn't get build fingerprint";
  } else {
    os_version_build_.push_back(' ');
    os_version_build_ += build_string;
    os_version_full_.push_back(' ');
    os_version_full_ += build_string;
  }
#endif  // OS_ANDROID

  INITIALIZATION_STATE_SET_VALID(initialized_);
}

CPUArchitecture SystemSnapshotLinux::GetCPUArchitecture() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

#if defined(ARCH_CPU_X86_FAMILY)
  return process_reader_->Is64Bit() ? kCPUArchitectureX86_64
                                    : kCPUArchitectureX86;
#else
#error port to your architecture
#endif
}

uint32_t SystemSnapshotLinux::CPURevision() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

#if defined(ARCH_CPU_X86_FAMILY)
  return cpuid_.Revision();
#else
#error port to your architecture
#endif
}

uint8_t SystemSnapshotLinux::CPUCount() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  std::string contents;
  if (!LoggingReadEntireFile(base::FilePath("/sys/devices/system/cpu/online"),
                             &contents)) {
    return 0;
  }
  if (contents.back() != '\n') {
    LOG(ERROR) << "format error";
    return 0;
  }
  contents.pop_back();

  uint8_t count = 0;
  std::vector<std::string> ranges = SplitString(contents, ',');
  for (const auto& range : ranges) {
    std::string left;
    std::string right;
    if (SplitStringFirst(range, '-', &left, &right)) {
      unsigned int start;
      unsigned int end;
      if (!StringToUint(base::StringPiece(left), &start) ||
          !StringToUint(base::StringPiece(right), &end) || end <= start) {
        LOG(ERROR) << "format error:" << left << ":" << right << ";";
        return 0;
      }
      count += end - start + 1;
    } else {
      unsigned int cpuno;
      if (!StringToUint(base::StringPiece(range), &cpuno)) {
        LOG(ERROR) << "format error";
        return 0;
      }
      ++count;
    }
  }
  return count;
}

std::string SystemSnapshotLinux::CPUVendor() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

#if defined(ARCH_CPU_X86_FAMILY)
  return cpuid_.Vendor();
#else
#error port to your architecture
#endif
}

void SystemSnapshotLinux::CPUFrequency(uint64_t* current_hz,
                                       uint64_t* max_hz) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  *current_hz = 0;
  *max_hz = 0;

  int cpu = getcpu();
  if (cpu < 0) {
    return;
  }

  ReadFreqFile(
      base::StringPrintf(
          "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", cpu),
      current_hz);

  ReadFreqFile(
      base::StringPrintf(
          "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq", cpu),
      max_hz);
}

uint32_t SystemSnapshotLinux::CPUX86Signature() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

#if defined(ARCH_CPU_X86_FAMILY)
  return cpuid_.Signature();
#else
  NOTREACHED();
  return 0;
#endif
}

uint64_t SystemSnapshotLinux::CPUX86Features() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

#if defined(ARCH_CPU_X86_FAMILY)
  return cpuid_.Features();
#else
  NOTREACHED();
  return 0;
#endif
}

uint64_t SystemSnapshotLinux::CPUX86ExtendedFeatures() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return cpuid_.ExtendedFeatures();
}

uint32_t SystemSnapshotLinux::CPUX86Leaf7Features() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

#if defined(ARCH_CPU_X86_FAMILY)
  return cpuid_.Leaf7Features();
#else
  NOTREACHED();
  return 0;
#endif
}

bool SystemSnapshotLinux::CPUX86SupportsDAZ() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

#if defined(ARCH_CPU_X86_FAMILY)
  // The correct way to check for denormals-as-zeros (DAZ) support is to examine
  // mxcsr mask, which can be done with fxsave. See Intel Software Developer’s
  // Manual, Volume 1: Basic Architecture (253665-051), 11.6.3 “Checking for the
  // DAZ Flag in the MXCSR Register”. Note that since this function tests for
  // DAZ support in the CPU, it checks the mxcsr mask. Testing mxcsr would
  // indicate whether DAZ is actually enabled, which is a per-thread context
  // concern.

  // Test for fxsave support.
  uint64_t features = CPUX86Features();
  if (!(features & (UINT64_C(1) << 24))) {
    return false;
  }

// Call fxsave.
#if defined(ARCH_CPU_X86)
  CPUContextX86::Fxsave fxsave __attribute__((aligned(16))) = {};
#elif defined(ARCH_CPU_X86_64)
  CPUContextX86_64::Fxsave fxsave __attribute__((aligned(16))) = {};
#endif
  static_assert(sizeof(fxsave) == 512, "fxsave size");
  static_assert(offsetof(decltype(fxsave), mxcsr_mask) == 28,
                "mxcsr_mask offset");
  asm("fxsave %0" : "=m"(fxsave));

  // Test the DAZ bit.
  return fxsave.mxcsr_mask & (1 << 6);
#else
  NOTREACHED();
  return false;
#endif  // ARCH_CPU_X86_FMAILY
}

SystemSnapshot::OperatingSystem SystemSnapshotLinux::GetOperatingSystem()
    const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return kOperatingSystemLinux;
}

bool SystemSnapshotLinux::OSServer() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return true;
}

void SystemSnapshotLinux::OSVersion(int* major,
                                    int* minor,
                                    int* bugfix,
                                    std::string* build) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *major = os_version_major_;
  *minor = os_version_minor_;
  *bugfix = os_version_bugfix_;
  build->assign(os_version_build_);
}

std::string SystemSnapshotLinux::OSVersionFull() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return os_version_full_;
}

std::string SystemSnapshotLinux::MachineDescription() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::string();  // TODO(jperaza): anything to do?
}

bool SystemSnapshotLinux::NXEnabled() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return cpuid_.NXEnabled();
}

void SystemSnapshotLinux::TimeZone(DaylightSavingTimeStatus* dst_status,
                                   int* standard_offset_seconds,
                                   int* daylight_offset_seconds,
                                   std::string* standard_name,
                                   std::string* daylight_name) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  internal::TimeZone(*snapshot_time_,
                     dst_status,
                     standard_offset_seconds,
                     daylight_offset_seconds,
                     standard_name,
                     daylight_name);
}

void SystemSnapshotLinux::ReadKernelVersion(const std::string& version_string) {
  std::vector<std::string> versions = SplitString(version_string, '.');

  if (versions.size() < 3) {
    LOG(WARNING) << "format error";
    return;
  }

  if (!StringToInt(base::StringPiece(versions[0]), &os_version_major_)) {
    LOG(WARNING) << "no kernel version";
    return;
  }
  DCHECK_GE(os_version_major_, 3);

  if (!StringToInt(base::StringPiece(versions[1]), &os_version_minor_)) {
    LOG(WARNING) << "no major revision";
    return;
  }
  DCHECK_GE(os_version_minor_, 0);

  size_t minor_rev_end = versions[2].find_first_not_of("0123456789");
  if (minor_rev_end == std::string::npos) {
    minor_rev_end = versions[2].size();
  }
  if (!StringToInt(base::StringPiece(versions[2].c_str(), minor_rev_end),
                   &os_version_bugfix_)) {
    LOG(WARNING) << "no minor revision";
    return;
  }
  DCHECK_GE(os_version_bugfix_, 0);

  os_version_build_ = versions[2].substr(minor_rev_end);
}

}  // namespace internal
}  // namespace crashpad
