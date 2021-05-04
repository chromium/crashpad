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

#include "util/ios/ios_intermediatedump_reader.h"

#include <fcntl.h>
#include <sys/stat.h>

#include "base/logging.h"

namespace crashpad {
namespace internal {

bool IOSIntermediatedumpReader::Initialize(const base::FilePath& dump_path) {
  reader_ = std::make_unique<crashpad::FileReader>();
  if (!reader_->Open(dump_path)) {
    return false;
  }

  // Don't initialize empty files.
  if (reader_->Seek(0, SEEK_END) == 0)
    return false;

  return true;
}

bool IOSIntermediatedumpReader::Parse() {
  if (!IOSIntermediatedumpMap::Parse(reader_.get(), minidump_)) {
    DLOG(ERROR) << "Intermediate dump parsing failed";
    return false;
  }

  // Useful for seeing what keys have been parsed.
#if !defined(NDEBUG) && 0
  minidump_.DebugDump();
#endif
  return true;
}

}  // namespace internal
}  // namespace crashpad
