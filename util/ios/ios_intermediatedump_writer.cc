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

#include "util/ios/ios_intermediatedump_writer.h"

#include <fcntl.h>
#include <stdint.h>

#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"
#include "util/file/file_io.h"
#include "util/ios/scoped_vm_read.h"

namespace crashpad {
namespace internal {

#define PROTECTION_CLASS_D 4

bool IOSIntermediatedumpWriter::Open(const base::FilePath& path) {
  // Set data protection class D (No protection). A file with this type of
  // protection can be read from or written to at any time.
  // See:
  // https://support.apple.com/guide/security/data-protection-classes-secb010e978a/web
  fd_ = HANDLE_EINTR(open_dprotected_np(path.value().c_str(),
                                        O_WRONLY | O_CREAT | O_TRUNC,
                                        PROTECTION_CLASS_D,
                                        0 /* dpflags */,
                                        0644 /* mode */));
  if (fd_ < 0) {
    PLOG(ERROR) << "open intermediate dump " << path.value();
    return false;
  }

  return LoggingLockFile(
             fd_, FileLocking::kExclusive, FileLockingBlocking::kNonBlocking) ==
         FileLockingResult::kSuccess;
}

bool IOSIntermediatedumpWriter::Close() {
  static uint8_t t = DOCUMENT_END;
  // Note: LoggingCloseFile is not `RUNS-DURING-CRASH` safe, but at this point
  // everything has been written so if things explode it doesn't matter.
  return LoggingWriteFile(fd_, &t, sizeof(uint8_t)) && LoggingCloseFile(fd_);
}

bool IOSIntermediatedumpWriter::ArrayMapStart() {
  static uint8_t t = MAP_START;
  return LoggingWriteFile(fd_, &t, sizeof(uint8_t));
}

bool IOSIntermediatedumpWriter::MapStart(IntermediateDumpKey key) {
  static uint8_t t = MAP_START;
  return LoggingWriteFile(fd_, &t, sizeof(uint8_t)) &&
         LoggingWriteFile(fd_, &key, sizeof(key));
}

bool IOSIntermediatedumpWriter::ArrayStart(IntermediateDumpKey key) {
  static uint8_t t = ARRAY_START;
  return LoggingWriteFile(fd_, &t, sizeof(uint8_t)) &&
         LoggingWriteFile(fd_, &key, sizeof(key));
}

bool IOSIntermediatedumpWriter::MapEnd() {
  static uint8_t t = MAP_END;
  return LoggingWriteFile(fd_, &t, sizeof(uint8_t));
}

bool IOSIntermediatedumpWriter::ArrayEnd() {
  static uint8_t t = ARRAY_END;
  return LoggingWriteFile(fd_, &t, sizeof(uint8_t));
}

bool IOSIntermediatedumpWriter::AddPropertyInternal(IntermediateDumpKey key,
                                                    const char* value,
                                                    size_t value_length) {
  ScopedVMRead<char> vmread;
  if (!vmread.Read(value, value_length))
    return false;
  uint8_t t = PROPERTY;
  return LoggingWriteFile(fd_, &t, sizeof(t)) &&
         LoggingWriteFile(fd_, &key, sizeof(key)) &&
         LoggingWriteFile(fd_, &value_length, sizeof(size_t)) &&
         LoggingWriteFile(fd_, vmread.get(), value_length);
}

}  // namespace internal
}  // namespace crashpad
