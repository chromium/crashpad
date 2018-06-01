// Copyright 2018 The Crashpad Authors. All rights reserved.
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

#include "snapshot/sanitized/sanitization_information.h"

namespace crashpad {

namespace {

template <typename Pointer>
bool ReadWhitelist(ProcessMemoryRange* memory,
                   VMAddress whitelist_address,
                   std::vector<std::string>* whitelist) {
  if (!whitelist_address) {
    return true;
  }

  Pointer name_address;
  while (memory->Read(whitelist_address, sizeof(name_address), &name_address)) {
    if (!name_address) {
      return true;
    }

    std::string name;
    if (!memory->ReadCStringSizeLimited(name_address,
                                       1024,
                                       &name)) {
      return false;
    }
    whitelist->push_back(name);
    whitelist_address += sizeof(Pointer);
  }

  return false;
}

}  // namespace

bool ReadSanitizationInfo(ProcessMemoryRange* memory,
                          VMAddress sanitization_info_address,
                          std::vector<std::string>* whitelist,
                          VMAddress* target_module_address,
                          bool* sanitize_stacks) {
  SanitizationInformation sanitization_info;
  if (!memory->Read(sanitization_info_address,
                    sizeof(sanitization_info),
                    &sanitization_info)) {
    return false;
  }

  std::vector<std::string> local_whitelist;
  if (!(memory->Is64Bit() ?
        ReadWhitelist<uint64_t>(memory,
                                sanitization_info.annotations_whitelist_address,
                                &local_whitelist)
      : ReadWhitelist<uint32_t>(memory,
                                sanitization_info.annotations_whitelist_address,
                                &local_whitelist))) {
    return false;
  }

  whitelist->swap(local_whitelist);
  *target_module_address = sanitization_info.target_module_address;
  *sanitize_stacks = sanitization_info.sanitize_stacks != 0;
  return true;
}

}  // namespace crashpad
