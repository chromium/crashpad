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

#ifndef CRASHPAD_UTIL_IOS_IOS_MINIDUMP_WRITER_H_
#define CRASHPAD_UTIL_IOS_IOS_MINIDUMP_WRITER_H_

#include <unistd.h>

#include "base/macros.h"

namespace crashpad {
namespace internal {

enum CommandType {
  MAP_START = 0x01,
  MAP_END = 0x02,
  ARRAY_START = 0x03,
  ARRAY_END = 0x04,
  PROPERTY = 0x05
};

class IOSMinidumpWriterUtil final {
 public:
  static void ArrayMapStart(int fd);

  static void MapStart(int fd, const char* name);

  static void ArrayStart(int fd, const char* name);

  static void MapEnd(int fd);

  static void ArrayEnd(int fd);

  static void Property(int fd,
                       const char* name,
                       const void* value,
                       size_t value_length);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(IOSMinidumpWriterUtil);
};

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_UTIL_IOS_IOS_MINIDUMP_WRITER_H_
