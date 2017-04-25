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

#include "util/linux/memory_map.h"

#include <linux/kdev_t.h>
#include <stdio.h>
#include <string.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "util/file/delimited_file_reader.h"
#include "util/file/file_reader.h"
#include "util/stdlib/string_number_conversion.h"

namespace crashpad {

namespace {

// This function is used in this file specfically for signed or unsigned longs.
// longs are typically either int or int64 sized, but pointers to longs are not
// automatically coerced to pointers to ints when they are the same size.
// Simply adding a StringToNumber for longs doesn't work since sometimes long
// and int64_t are actually the same type, resulting in a redefinition error.
template <typename Type>
bool LocalStringToNumber(const base::StringPiece& string, Type* number) {
  static_assert(sizeof(Type) == sizeof(int) || sizeof(Type) == sizeof(int64_t),
                "Unexpected Type size");

  if (sizeof(Type) == sizeof(int)) {
    return std::numeric_limits<Type>::is_signed
               ? StringToNumber(string, reinterpret_cast<int*>(number))
               : StringToNumber(string,
                                reinterpret_cast<unsigned int*>(number));
  } else {
    return std::numeric_limits<Type>::is_signed
               ? StringToNumber(string, reinterpret_cast<int64_t*>(number))
               : StringToNumber(string, reinterpret_cast<uint64_t*>(number));
  }
}

template <typename Type>
bool HexStringToNumber(const std::string& string, Type* number) {
  return LocalStringToNumber("0x" + string, number);
}

}  // namespace

MemoryMap::Mapping::Mapping()
    : name(),
      range(false, 0, 0),
      offset(0),
      device(0),
      inode(0),
      readable(false),
      writable(false),
      executable(false),
      shareable(false) {}

MemoryMap::MemoryMap() : mappings_(), initialized_() {}

MemoryMap::~MemoryMap() {}

bool MemoryMap::Initialize(pid_t pid) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  char path[32];
  snprintf(path, sizeof(path), "/proc/%d/maps", pid);
  FileReader maps_file;
  if (!maps_file.Open(base::FilePath(path))) {
    return false;
  }
  DelimitedFileReader maps_file_reader(&maps_file);

  DelimitedFileReader::Result result;
  std::string field;
  while ((result = maps_file_reader.GetDelim('-', &field)) ==
         DelimitedFileReader::Result::kSuccess) {
    field.pop_back();

    LinuxVMAddress start_address;
    if (!HexStringToNumber(field, &start_address)) {
      LOG(ERROR) << "format error";
      return false;
    }

    LinuxVMAddress end_address;
    if (maps_file_reader.GetDelim(' ', &field) !=
            DelimitedFileReader::Result::kSuccess ||
        (field.pop_back(), !HexStringToNumber(field, &end_address))) {
      LOG(ERROR) << "format error";
      return false;
    }
    if (end_address <= start_address) {
      LOG(ERROR) << "format error";
      return false;
    }

    // TODO(jperaza): set bitness properly
#if defined(ARCH_CPU_64_BITS)
    const bool is_64_bit = true;
#else
    const bool is_64_bit = false;
#endif

    Mapping mapping;
    mapping.range.SetRange(
        is_64_bit, start_address, end_address - start_address);

    if (maps_file_reader.GetDelim(' ', &field) !=
            DelimitedFileReader::Result::kSuccess ||
        (field.pop_back(), field.size() != 4)) {
      LOG(ERROR) << "format error";
      return false;
    }
#define SET_FIELD(actual_c, outval, true_chars, false_chars) \
  do {                                                       \
    if (strchr(true_chars, actual_c)) {                      \
      *outval = true;                                        \
    } else if (strchr(false_chars, actual_c)) {              \
      *outval = false;                                       \
    } else {                                                 \
      LOG(ERROR) << "format error";                          \
      return false;                                          \
    }                                                        \
  } while (false)
    SET_FIELD(field[0], &mapping.readable, "r", "-");
    SET_FIELD(field[1], &mapping.writable, "w", "-");
    SET_FIELD(field[2], &mapping.executable, "x", "-");
    SET_FIELD(field[3], &mapping.shareable, "sS", "p");
#undef SET_FIELD

    if (maps_file_reader.GetDelim(' ', &field) !=
            DelimitedFileReader::Result::kSuccess ||
        (field.pop_back(), !HexStringToNumber(field, &mapping.offset))) {
      LOG(ERROR) << "format error";
      return false;
    }

    int major, minor;
    if (maps_file_reader.GetDelim(' ', &field) !=
            DelimitedFileReader::Result::kSuccess ||
        (field.pop_back(), field.size() != 5) ||
        !HexStringToNumber(field.substr(0, 2), &major) ||
        !HexStringToNumber(field.substr(3, 2), &minor)) {
      LOG(ERROR) << "format error";
      return false;
    }
    mapping.device = MKDEV(major, minor);

    if (maps_file_reader.GetDelim(' ', &field) !=
            DelimitedFileReader::Result::kSuccess ||
        (field.pop_back(), !LocalStringToNumber(field, &mapping.inode))) {
      LOG(ERROR) << "format error";
      return false;
    }

    if (maps_file_reader.GetDelim('\n', &field) !=
        DelimitedFileReader::Result::kSuccess) {
      LOG(ERROR) << "format error";
      return false;
    }
    if (field.back() != '\n') {
      LOG(ERROR) << "format error";
      return false;
    }
    field.pop_back();

    mappings_.push_back(mapping);

    size_t path_start = field.find_first_not_of(' ');
    if (path_start != std::string::npos) {
      mappings_.back().name = field.substr(path_start);
    }
  }
  if (result != DelimitedFileReader::Result::kEndOfFile) {
    return false;
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

const MemoryMap::Mapping* MemoryMap::FindMapping(LinuxVMAddress address) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  for (const auto& mapping : mappings_) {
    if (mapping.range.Base() <= address && mapping.range.End() > address) {
      return &mapping;
    }
  }
  return nullptr;
}

}  // namespace crashpad
