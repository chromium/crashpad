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

#include <type_traits>

namespace crashpad {

class CrashpadInfoReader::InfoContainer {
 public:
  virtual ~InfoContainer() = default;

 protected:
  InfoContainer() = default;
};

template <class Traits>
class CrashpadInfoReader::InfoContainerSpecific : public InfoContainer {
 public:
  InfoContainerSpecific() : InfoContainer() {}
  ~InfoContainerSpecific() = default;

  struct CrashpadInfo {
    uint32_t signature;
    uint32_t size;
    uint32_t version;
    uint32_t indirectly_referenced_memory_cap;
    uint32_t padding_0;
    TriState crashpad_handler_behavior;
    TriState system_crash_reporter_forwarding;
    TriState gather_indirectly_referenced_memory;
    uint8_t padding_1;
    typename Traits::Address extra_memory_ranges;
    typename Traits::Address simple_annotations;
    typename Traits::Address user_data_minidump_stream_head;
    typename Traits::Address annotations_list;
  } info;
};

CrashpadInfoReader::CrashpadInfoReader() : container_(), is_64_bit_(false) {}

CrashpadInfoReader::~CrashpadInfoReader() = default;

bool CrashpadInfoReader::Initialize(const ProcessMemoryRange* memory,
                                    VMAddress address) {
  is_64_bit_ = memory->Is64Bit();

#define READ_INFO(traits)                                                     \
  ({                                                                          \
    bool result;                                                              \
    do {                                                                      \
      auto new_container = std::make_unique<InfoContainerSpecific<traits>>(); \
      result = memory->Read(                                                  \
          address, sizeof(new_container->info), &new_container->info);        \
      if (result) {                                                           \
        container_ = std::move(new_container);                                \
      }                                                                       \
    } while (0);                                                              \
    result;                                                                   \
  })

  return is_64_bit_ ? READ_INFO(Traits64) : READ_INFO(Traits32);
#undef READ_INFO
}

#define GET_MEMBER(name)                                                      \
  (is_64_bit_                                                                 \
       ? reinterpret_cast<InfoContainerSpecific<Traits64>*>(container_.get()) \
             ->info.name                                                      \
       : reinterpret_cast<InfoContainerSpecific<Traits32>*>(container_.get()) \
             ->info.name)

#define DEFINE_GETTER(type, method, member) \
  type CrashpadInfoReader::method() { return GET_MEMBER(member); }

DEFINE_GETTER(TriState, CrashpadHandlerBehavior, crashpad_handler_behavior);
DEFINE_GETTER(TriState,
              SystemCrashReporterForwarding,
              system_crash_reporter_forwarding);
DEFINE_GETTER(TriState,
              GatherIndirectlyReferencedMemory,
              gather_indirectly_referenced_memory);

DEFINE_GETTER(VMAddress, ExtraMemoryRanges, extra_memory_ranges);
DEFINE_GETTER(VMAddress, SimpleAnnotations, simple_annotations);
DEFINE_GETTER(VMAddress, AnnotationsList, annotations_list);
DEFINE_GETTER(VMAddress,
              UserDataMinidumpStreamHead,
              user_data_minidump_stream_head);

#undef DEFINE_GETTER
#undef GET_MEMBER

}  // namespace crashpad
