// Copyright 2020 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_UTIL_IOS_IOS_INTERMEDIATEDUMP_WRITER_H_
#define CRASHPAD_UTIL_IOS_IOS_INTERMEDIATEDUMP_WRITER_H_

#include <unistd.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "util/mach/mach_extensions.h"

namespace crashpad {
namespace internal {

template <typename T>
class ScopedVMRead {
 public:
  explicit ScopedVMRead(const T* value, size_t value_length = sizeof(T)) {
    reset(value, value_length);
  }
  explicit ScopedVMRead(uint64_t value, size_t value_length = sizeof(T)) {
    reset(reinterpret_cast<T*>(value), value_length);
  }

  ~ScopedVMRead() {
    if (data_) {
      release();
    }
  }

  void reset(const void* value, size_t value_length = sizeof(T)) {
    if (data_) {
      release();
    }
    mach_vm_address_t page_region_address = mach_vm_trunc_page(value);
    mach_vm_size_t page_region_size = mach_vm_round_page(
        (vm_address_t)value - page_region_address + value_length);
    kern_return_t kr = vm_read(mach_task_self(),
                               page_region_address,
                               page_region_size,
                               &vm_read_data_,
                               &vm_read_data_count_);
    if (kr == KERN_SUCCESS) {
      data_ = vm_read_data_ + ((vm_address_t)value - page_region_address);
    } else {
      PLOG(WARNING) << "vm_read";
    }
  }

  T* operator->() const { return get(); }
  T* get() const { return (T*)data_; }

  bool is_valid() const { return !!data_ && vm_read_data_count_ > 0; }

 private:
  void release() {
    vm_deallocate(mach_task_self(), vm_read_data_, vm_read_data_count_);
    data_ = 0;
  }
  vm_address_t data_ = 0;
  vm_address_t vm_read_data_;
  mach_msg_type_number_t vm_read_data_count_ = 0;
  DISALLOW_COPY_AND_ASSIGN(ScopedVMRead);
};

enum CommandType : uint8_t {
  MAP_START = 0x01,
  MAP_END = 0x02,
  ARRAY_START = 0x03,
  ARRAY_END = 0x04,
  PROPERTY = 0x05,
  DOCUMENT_END = 0x06
};

// Declare tokens for intermediate format as enum DumpKey.  Use
// |INTERMEDIATE_DUMP_KEYS|'s so it's easier to log keys in debug for testing.
// These values are persisted to the intermediate crash dump file.. Entries
// should not be renumbered and numeric values should never be reused.
// clang-format off
#define INTERMEDIATE_DUMP_KEYS(TD) \
  TD(kInvalid, 0) \
  TD(kVersion, 1) \
  TD(kMachException, 1000) \
    TD(kCode, 1001) \
    TD(kCodeCount, 1002) \
    TD(kException, 1003) \
    TD(kFlavor, 1004) \
    TD(kState, 1005) \
    TD(kStateCount, 1006) \
  TD(kSignalException, 2000) \
    TD(kSignalNumber, 2001) \
    TD(kSignalCode, 2002) \
    TD(kSignalAddress, 2003) \
  TD(kNSException, 2500) \
  TD(kModules, 3000) \
    TD(kAddress, 3001) \
    TD(kFileType, 3002) \
    TD(kName, 3003) \
    TD(kSize, 3004) \
    TD(kDylibCurrentVersion, 3005) \
    TD(kSourceVersion, 3006) \
    TD(kTimestamp, 3007) \
    TD(kUUID, 3008) \
    TD(kAnnotationObjects, 3009) \
    TD(kAnnotationsSimpleMap, 3010) \
    TD(kAnnotationsVector, 3011) \
    TD(kAnnotationType, 3012) \
    TD(kAnnotationName, 3013) \
    TD(kAnnotationValue, 3014) \
    TD(kAnnotationsCrashInfo, 3015) \
    TD(kAnnotationsCrashInfoMessage1, 3016) \
    TD(kAnnotationsCrashInfoMessage2, 3017) \
    TD(kAnnotationsDyldErrorString, 3018) \
  TD(kProcessInfo, 4000) \
    TD(kParentPID, 4001) \
    TD(kPID, 4002) \
    TD(kStartTime, 4003) \
    TD(kSnapshotTime, 4004) \
    TD(kTaskBasicInfo, 4005) \
    TD(kTaskThreadTimes, 4006) \
    TD(kSystemTime, 4007) \
    TD(kUserTime, 4008) \
  TD(kSystemInfo, 5000) \
    TD(kCpuCount, 5001) \
    TD(kCpuVendor, 5002) \
    TD(kDaylightName, 5003) \
    TD(kDaylightOffsetSeconds, 5004) \
    TD(kHasDaylightSavingTime, 5005) \
    TD(kIsDaylightSavingTime, 5006) \
    TD(kMachineDescription, 5007) \
    TD(kOSVersionBugfix, 5008) \
    TD(kOSVersionBuild, 5009) \
    TD(kOSVersionMajor, 5010) \
    TD(kOSVersionMinor, 5011) \
    TD(kPageSize, 5012) \
    TD(kStandardName, 5013) \
    TD(kStandardOffsetSeconds, 5014) \
    TD(kVMStat, 5015) \
    TD(kActive, 5016) \
    TD(kFree, 5017) \
    TD(kInactive, 5018) \
    TD(kWired, 5019) \
  TD(kThreads, 6000) \
    TD(kDebugState, 6001) \
    TD(kFloatState, 6002) \
    TD(kThreadState, 6003) \
    TD(kPriority, 6004) \
    TD(kStackRegionAddress, 6005) \
    TD(kStackRegionData, 6006) \
    TD(kSuspendCount, 6007) \
    TD(kThreadID, 6008) \
    TD(kThreadDataAddress, 6009) \
    TD(kThreadUncaughtNSExceptionFrames, 6010)


enum class IntermediateDumpKey : uint64_t {
  #define X(NAME, VALUE) NAME = VALUE,
  INTERMEDIATE_DUMP_KEYS(X)
  #undef X
};
// clang-format on

class IOSIntermediatedumpWriter final {
 public:
  IOSIntermediatedumpWriter() = default;
  bool Open(const base::FilePath& path);
  bool Close();

  class ScopedMap {
   public:
    explicit ScopedMap(IOSIntermediatedumpWriter* writer,
                       IntermediateDumpKey key)
        : writer_(writer) {
      writer->MapStart(key);
    }
    explicit ScopedMap(IOSIntermediatedumpWriter* writer) : writer_(writer) {
      writer->ArrayMapStart();
    }
    ~ScopedMap() { writer_->MapEnd(); }

   private:
    IOSIntermediatedumpWriter* writer_;
    DISALLOW_COPY_AND_ASSIGN(ScopedMap);
  };

  class ScopedArray {
   public:
    explicit ScopedArray(IOSIntermediatedumpWriter* writer,
                         IntermediateDumpKey key)
        : writer_(writer) {
      writer->ArrayStart(key);
    }
    ~ScopedArray() { writer_->ArrayEnd(); }

   private:
    IOSIntermediatedumpWriter* writer_;
    DISALLOW_COPY_AND_ASSIGN(ScopedArray);
  };

  void AddProperty(IntermediateDumpKey key,
                   const void* value,
                   size_t value_length);

  static size_t ThreadStateLengthForFlavor(thread_state_flavor_t flavor);

 private:
  void ArrayStart(IntermediateDumpKey key);
  void MapStart(IntermediateDumpKey key);
  void ArrayMapStart();
  void ArrayEnd();
  void MapEnd();
  int fd_;
  DISALLOW_COPY_AND_ASSIGN(IOSIntermediatedumpWriter);
};

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_UTIL_IOS_IOS_INTERMEDIATEDUMP_WRITER_H_
