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

#include <stdio.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "util/file/delimited_file_reader.h"
#include "util/file/file_reader.h"
#include "util/stdlib/string_number_conversion.h"

namespace crashpad {

MemoryMap::Mapping::Mapping()
    : name(),
      start_address(0),
      end_address(0),
      readable(false),
      writable(false),
      executable(false),
      shared(false) {}

MemoryMap::MemoryMap()
    : mappings_() {}

MemoryMap::~MemoryMap() {}

bool MemoryMap::Initialize(pid_t pid) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  char path[32];
  snprintf(path, sizeof(path), "/proc/%d/maps", pid);
  FileReader maps_file;
  if (!maps_file.Open(base::FilePath(path))) {
    LOG(ERROR) << "Couldn't open file";
    return false;
  }
  DelimitedFileReader maps_file_reader(&maps_file);

  DelimitedFileReader::Result result;
  std::string field;
  while ((result = maps_file_reader.GetDelim('-', &field)) ==
          DelimitedFileReader::Result::kSuccess) {
    Mapping mapping;

    field.pop_back();
    if (!StringToNumber("0x" + field, &mapping.start_address)) {
      LOG(ERROR) << "format error:" << field;
      return false;
    }

    if (maps_file_reader.GetDelim(' ', &field) !=
        DelimitedFileReader::Result::kSuccess ||
        (field.pop_back(), !StringToNumber("0x" + field, &mapping.end_address))) {
      LOG(ERROR) << "format error";
      return false;
    }

    if (maps_file_reader.GetDelim(' ', &field) !=
        DelimitedFileReader::Result::kSuccess ||
        (field.pop_back(), field.size() != 4)) {
      LOG(ERROR) << "format error";
      return false;
    }
    if (field[0] == 'r') {
      mapping.readable = true;
    } else if (field[0] != '-') {
      LOG(ERROR) << "format error";
      return false;
    }
    if (field[1] == 'w') {
      mapping.writable = true;
    } else if (field[1] != '-') {
      LOG(ERROR) << "format error";
      return false;
    }
    if (field[2] == 'x') {
      mapping.executable = true;
    } else if (field[2] != '-') {
      LOG(ERROR) << "format error";
      return false;
    }
    if (field[3] == 's') {
      mapping.shared = true;
    } else if (field[3] != 'p') {
      LOG(ERROR) << "format error";
      return false;
    }

    // offset
    if (maps_file_reader.GetDelim(' ', &field) !=
        DelimitedFileReader::Result::kSuccess) {
      LOG(ERROR) << "format error";
      return false;
    }

    // device
    if (maps_file_reader.GetDelim(' ', &field) !=
        DelimitedFileReader::Result::kSuccess) {
      LOG(ERROR) << "format error";
      return false;
    }

    // inode
    if (maps_file_reader.GetDelim(' ', &field) !=
        DelimitedFileReader::Result::kSuccess) {
      LOG(ERROR) << "format error";
      return false;
    }

    if (maps_file_reader.GetDelim('\n', &field) !=
        DelimitedFileReader::Result::kSuccess) {
      LOG(ERROR) << "format error";
      return false;
    }
    if (field.back() == '\n') {
      field.pop_back();
    }

    size_t path_start = 0;
    while (path_start < field.size() && field[path_start] == ' ') {
      ++path_start;
    }
    if (path_start != field.size()) {
      std::string name = field.substr(path_start);
      mapping.name.swap(name);
    }

    mappings_.push_back(mapping);
  }
  if (result != DelimitedFileReader::Result::kEndOfFile) {
    return false;
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

const MemoryMap::Mapping* MemoryMap::FindMapping(LinuxVMAddress address) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  for (size_t index = 0; index < mappings_.size(); ++index) {
    if (mappings_[index].start_address <= address &&
        mappings_[index].end_address > address) {
      return &mappings_[index];
    }
  }
  return nullptr;
}

}  // namespace crashpad
