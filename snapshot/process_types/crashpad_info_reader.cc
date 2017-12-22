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

#include "snapshot/process_types/crashpad_info_reader.h"

#include "util/misc/traits.h"
#include "util/misc/tri_state.h"

namespace crashpad {

struct CrashpadInfoReader::CrashpadInfo {
public:
 CrashpadInfo() = default;
 virtual ~CrashpadInfo() = default;
};

template <class Traits>
struct CrashpadInfoReader::CrashpadInfoSpecific : public CrashpadInfo {
  CrashpadInfoSpecific() = default;
  ~CrashpadInfoSpecific() override = default;

  uint32_t signature;
  uint32_t size;
  uint32_t version;
  uint32_t indirectly_referenced_memory_cap;
  uint32_t padding_0;
  TriState crashpad_handler_behavior;
  TriState system_crash_reporter_forwarding;
  TriState gather_indirectly_referenced_memory;
  uint8_t padding_1;
  typename Traits::Address extra_address_ranges;
  typename Traits::Address simple_annotations;
  typename Traits::Address user_data_minidump_stream_head;
  typename Traits::Address annotations_list;
};

bool CrashpadInfoReader::Initialize(const ProcessMemoryRange* memory,
                               VMAddress address) {
  is_64_bit_ = memory->Is64Bit();

#define READ_INFO(traits) ({ \
  bool result; \
  do { \
    info_ = std::make_unique<CrashpadInfoSpecific<traits>>(); \
    result = memory->Read(address, sizeof(CrashpadInfoSpecific<traits>), info_.get()); \
  } while(0); \
  result; \
})

  return is_64_bit_ ? READ_INFO(Traits64) : READ_INFO(Traits32);
#undef READ_INFO
}

#define GET_MEMBER(name)  \
    is_64_bit_ ? reinterpret_cast<CrashpadInfoSpecific<Traits64>*>(info_.get())->name \
               : reinterpret_cast<CrashpadInfoSpecific<Traits32>*>(info_.get())->name; \

VMAddress CrashpadInfoReader::SimpleAnnotations() {
  return GET_MEMBER(simple_annotations);
}

#undef GET_MEMBER

}  // namespace crashpad


