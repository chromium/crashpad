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

#include "util/ios/ios_minidump_writer_util.h"

#include <stdint.h>
#include <string.h>

namespace crashpad {
namespace internal {

// static
void IOSMinidumpWriterUtil::ArrayMapStart(int fd) {
  uint8_t t = MAP_START;
  write(fd, &t, sizeof(uint8_t));
}

// static
void IOSMinidumpWriterUtil::MapStart(int fd, const char* name) {
  uint8_t t = MAP_START;
  write(fd, &t, sizeof(uint8_t));
  size_t name_length = strlen(name);
  write(fd, &name_length, sizeof(size_t));
  write(fd, name, name_length);
}

// static
void IOSMinidumpWriterUtil::ArrayStart(int fd, const char* name) {
  uint8_t t = ARRAY_START;
  write(fd, &t, sizeof(uint8_t));
  size_t name_length = strlen(name);
  write(fd, &name_length, sizeof(size_t));
  write(fd, name, name_length);
}

// static
void IOSMinidumpWriterUtil::MapEnd(int fd) {
  uint8_t t = MAP_END;
  write(fd, &t, sizeof(uint8_t));
}

// static
void IOSMinidumpWriterUtil::ArrayEnd(int fd) {
  uint8_t t = ARRAY_END;
  write(fd, &t, sizeof(uint8_t));
}

// static
void IOSMinidumpWriterUtil::Property(int fd,
                                     const char* name,
                                     const void* value,
                                     size_t value_length) {
  uint8_t t = PROPERTY;
  write(fd, &t, sizeof(t));
  size_t name_length = strlen(name);
  write(fd, &name_length, sizeof(size_t));
  write(fd, name, name_length);
  off_t value_length_offset = lseek(fd, 0, SEEK_CUR);
  write(fd, &value_length, sizeof(size_t));
  ssize_t actual_length = write(fd, value, value_length);
  if (actual_length != (ssize_t)value_length) {
    // Seek back and zero out this property.
    lseek(fd, value_length_offset, SEEK_SET);
    constexpr size_t zero = 0;
    actual_length = write(fd, &zero, sizeof(size_t));
  }
}

}  // namespace internal
}  // namespace crashpad
