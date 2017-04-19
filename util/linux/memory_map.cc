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

#include "base/files/file_path.h"
#include "base/logging.h"
#include "util/file/delimited_file_reader.h"
#include "util/file/file_reader.h"
#include "util/stdlib/string_number_conversion.h"

namespace crashpad {

MemoryMap::Mapping::Mapping()
    : name(),
      range(false, 0, 0),
      readable(false),
      writable(false),
      executable(false),
      shareable(false) {}

MemoryMap::MemoryMap() : mappings_() {}

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
    if (!StringToNumber("0x" + field, &start_address)) {
      LOG(ERROR) << "format error";
      return false;
    }

    LinuxVMAddress end_address;
    if (maps_file_reader.GetDelim(' ', &field) !=
            DelimitedFileReader::Result::kSuccess ||
        (field.pop_back(),
         !StringToNumber("0x" + field, &end_address))) {
      LOG(ERROR) << "format error";
      return false;
    }

    // TODO set bitness properly
    Mapping mapping;
    mapping.range.SetRange(true, start_address, end_address - start_address);

    if (maps_file_reader.GetDelim(' ', &field) !=
            DelimitedFileReader::Result::kSuccess ||
        (field.pop_back(), field.size() != 4)) {
      LOG(ERROR) << "format error";
      return false;
    }
#define SET_FIELD(index, name, true_c, false_c) \
    if (field[index] == true_c) {               \
      mapping.name = true;                      \
    } else if (field[index] != false_c) {       \
      LOG(ERROR) << "format error";             \
      return false;                             \
    }
    SET_FIELD(0, readable, 'r', '-');
    SET_FIELD(1, writable, 'w', '-');
    SET_FIELD(2, executable, 'x', '-');
    SET_FIELD(3, shareable, 's', 'p');
#undef SET_FIELD

    int64_t offset;
    static_assert(std::numeric_limits<off_t>::is_signed, "off_t isn't signed");
    if (maps_file_reader.GetDelim(' ', &field) !=
        DelimitedFileReader::Result::kSuccess ||
        (field.pop_back(), !StringToNumber("0x" + field, &offset)) ||
        offset < std::numeric_limits<off_t>::min() ||
        offset > std::numeric_limits<off_t>::max()) {
      LOG(ERROR) << "format error";
      return false;
    }
    mapping.offset = offset;

    int major, minor;
    if (maps_file_reader.GetDelim(' ', &field) !=
        DelimitedFileReader::Result::kSuccess ||
        field.size() != 6 ||
        !StringToNumber("0x" + field.substr(0, 2), &major) ||
        !StringToNumber("0x" + field.substr(3, 2), &minor)) {
      LOG(ERROR) << "format error";
      return false;
    }
    mapping.device = MKDEV(major, minor);

    uint64_t inode;
    static_assert(!std::numeric_limits<ino_t>::is_signed, "ino_t is signed");
    if (maps_file_reader.GetDelim(' ', &field) !=
        DelimitedFileReader::Result::kSuccess ||
        (field.pop_back(), !StringToNumber(field, &inode)) ||
        inode > std::numeric_limits<ino_t>::max()) {
      LOG(ERROR) << "format error";
      return false;
    }
    mapping.inode = inode;

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
