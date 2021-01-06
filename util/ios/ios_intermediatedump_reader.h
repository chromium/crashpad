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

#ifndef CRASHPAD_UTIL_IOS_IOS_INTERMEDIATEDUMP_READER_H_
#define CRASHPAD_UTIL_IOS_IOS_INTERMEDIATEDUMP_READER_H_

#include "base/files/file_path.h"
#include "util/ios/ios_intermediatedump_map.h"

namespace crashpad {
namespace internal {

//! \brief Used to read interim ios minidumps.
class IOSIntermediatedumpReader {
 public:
  IOSIntermediatedumpReader() = default;

  bool Initialize(const base::FilePath& dump_path);

  const IOSIntermediatedumpMap& RootMap() { return minidump_; }

 private:
  IOSIntermediatedumpMap minidump_;
};

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_UTIL_IOS_IOS_INTERMEDIATEDUMP_READER_H_
