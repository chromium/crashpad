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

size_t IOSMinidumpWriterUtil::ThreadStateLengthForFlavor(
    thread_state_flavor_t flavor) {
#if defined(ARCH_CPU_X86_64)
  switch (flavor) {
    case x86_THREAD_STATE:
      return sizeof(x86_thread_state_t);
    case x86_FLOAT_STATE:
      return sizeof(x86_float_state_t);
    case x86_DEBUG_STATE:
      return sizeof(x86_debug_state_t);
    case x86_THREAD_STATE32:
      return sizeof(x86_thread_state32_t);
    case x86_FLOAT_STATE32:
      return sizeof(x86_float_state32_t);
    case x86_DEBUG_STATE32:
      return sizeof(x86_debug_state32_t);
    default:
      return 0;
  }
#elif defined(ARCH_CPU_ARM64)
  switch (flavor) {
    case ARM_UNIFIED_THREAD_STATE:
      return sizeof(arm_unified_thread_state_t);
    case ARM_THREAD_STATE64:
      return sizeof(arm_thread_state64_t);
    case ARM_NEON_STATE64:
      return sizeof(arm_neon_state64_t);
    case ARM_DEBUG_STATE64:
      return sizeof(arm_debug_state64_t);
    default:
      return 0;
  }
#endif
}

}  // namespace internal
}  // namespace crashpad
