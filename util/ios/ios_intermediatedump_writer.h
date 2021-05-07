// Copyright 2021 The Crashpad Authors. All rights reserved.
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

#include "base/files/file_path.h"
#include "base/macros.h"

namespace crashpad {
namespace internal {

// Declare tokens for intermediate format as enum DumpKey. Use
// |INTERMEDIATE_DUMP_KEYS|'s so it's easier to log keys in debug for testing.
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
    TD(kThreadUncaughtNSExceptionFrames, 6010) \
    TD(kThreadContextMemoryRegions, 6011) \
    TD(kThreadContextMemoryRegionAddress, 6012) \
    TD(kThreadContextMemoryRegionData, 6013) \
// clang-format on


//! \brief They key for items in the intermediate dump file.
//!
//! These values are persisted to the intermediate crash dump file. Entries
//! should not be renumbered and numeric values should never be reused.
enum class IntermediateDumpKey : uint64_t {
#define X(NAME, VALUE) NAME = VALUE,
  INTERMEDIATE_DUMP_KEYS(X)
#undef X
};

//! \brief ....
//! Note: All methods are `RUNS-DURING-CRASH`.
class IOSIntermediatedumpWriter final {
 public:
  IOSIntermediatedumpWriter() = default;

  //! \brief Command instructions for the intermediate dump reader.
  enum CommandType : uint8_t {
    //! \brief Indicates a new map, followed by associated key.
    MAP_START = 0x01,

    //! \brief Indicates map is complete.
    MAP_END = 0x02,

    //! \brief Indicates a new array, followed by associated key.
    ARRAY_START = 0x03,

    //! \brief Indicates array is complete.
    ARRAY_END = 0x04,

    //! \brief Indicates a new property, followed by a key, length and value.
    PROPERTY = 0x05,

    //! \brief Indicates there is nothing left to parse.
    DOCUMENT_END = 0x06,
  };

  //! \brief Open and lock an intermediate dump file. This is the only method
  //!     in the writer class that is generally run outside of a crash.
  //!
  //! \param[in] path The path to the intermediate dump.
  //!
  //! \return On success, returns `true`, otherwise returns `false`.
  bool Open(const base::FilePath& path);

  //! \brief Completes writing the intermediate dump file and releases the
  //!     file handle.
  //!
  //! \return On success, returns `true`, otherwise returns `false`.
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

  template <typename T>
  bool AddProperty(IntermediateDumpKey key, const T* value, size_t count = 1) {
    return AddPropertyInternal(
        key, reinterpret_cast<const char*>(value), count * sizeof(T));
  }

  bool AddPropertyBytes(IntermediateDumpKey key,
                        const void* value,
                        size_t value_length) {
    return AddPropertyInternal(
        key, reinterpret_cast<const char*>(value), value_length);
  }

 private:
  bool AddPropertyInternal(IntermediateDumpKey key,
                           const char* value,
                           size_t value_length);

  bool ArrayStart(IntermediateDumpKey key);
  bool MapStart(IntermediateDumpKey key);
  bool ArrayMapStart();
  bool ArrayEnd();
  bool MapEnd();

  int fd_;
  DISALLOW_COPY_AND_ASSIGN(IOSIntermediatedumpWriter);
};

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_UTIL_IOS_IOS_INTERMEDIATEDUMP_WRITER_H_
