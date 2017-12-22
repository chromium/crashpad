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

#ifndef CRASHPAD_SNAPSHOT_LINUX_CRASHPAD_INFO_READER_H_
#define CRASHPAD_SNAPSHOT_LINUX_CRASHPAD_INFO_READER_H_

#include "base/macros.h"
#include "util/misc/address_types.h"
#include "util/process/process_memory_range.h"

namespace crashpad {

//! \brief A reader of CrashpadInfo
class CrashpadInfoReader {
 public:
  //! \brief Constructs the object.
  //!
  //! \param[in] memory The reader for the remote process.
  //!     contained within the remote process.
  CrashpadInfoReader();
  ~CrashpadInfoReader() {}

  bool Initialize(const ProcessMemoryRange* memory, VMAddress address);

  VMAddress SimpleAnnotations();

 private:
  struct CrashpadInfo;
  template <typename Traits>
  struct CrashpadInfoSpecific;

  std::unique_ptr<CrashpadInfo> info_;
  bool is_64_bit_;

  DISALLOW_COPY_AND_ASSIGN(CrashpadInfoReader);
};

}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_LINUX_CRASHPAD_INFO_READER_H_
