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

#include "snapshot/process_types/annotation_reader.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "client/annotation_list.h"
#include "client/simple_string_dictionary.h"
#include "gtest/gtest.h"
#include "test/multiprocess.h"
#include "util/file/file_io.h"
#include "util/misc/address_types.h"
#include "util/misc/as_underlying_type.h"
#include "util/misc/from_pointer_cast.h"
#include "util/process/process_memory_linux.h"
#include "util/process/process_memory_range.h"

namespace crashpad {
namespace test {
namespace {

void ExpectSimpleMap(const std::map<std::string, std::string>& map,
                     const SimpleStringDictionary& expected_map) {
  EXPECT_EQ(map.size(), expected_map.GetCount());
  for (const auto& pair : map) {
    EXPECT_EQ(pair.second, expected_map.GetValueForKey(pair.first));
  }
}

void ExpectAnnotationList(const std::vector<AnnotationSnapshot>& list,
                          AnnotationList& expected_list) {
  size_t index = 0;
  for (const Annotation* expected_annotation : expected_list) {
    const AnnotationSnapshot& annotation = list[index++];
    EXPECT_EQ(annotation.name, expected_annotation->name());
    EXPECT_EQ(annotation.type, AsUnderlyingType(expected_annotation->type()));
    // TODO(jperaza): value
  }
}

class ReadFromChildTest : public Multiprocess {
 public:
  ReadFromChildTest() : Multiprocess() {}

  ~ReadFromChildTest() {}

 private:
  void MultiprocessParent() {
    ProcessMemoryLinux memory;
    ASSERT_TRUE(memory.Initialize(ChildPID()));

#if defined(ARCH_CPU_64_BITS)
    constexpr bool am_64_bit = true;
#else
    constexpr bool am_64_bit = false;
#endif

    ProcessMemoryRange range;
    ASSERT_TRUE(range.Initialize(&memory, am_64_bit));

    AnnotationReader reader(&range);

    std::map<std::string, std::string> simple_map;
    ASSERT_TRUE(reader.SimpleMap(
        FromPointerCast<VMAddress>(&expected_simple_map_), &simple_map));
    ExpectSimpleMap(simple_map, expected_simple_map_);

    std::vector<AnnotationSnapshot> annotation_list;
    ASSERT_TRUE(reader.AnnotationList(
        FromPointerCast<VMAddress>(&expected_annotation_list_),
        &annotation_list));
    ExpectAnnotationList(annotation_list, expected_annotation_list_);
  }

  void MultiprocessChild() { CheckedReadFileAtEOF(ReadPipeHandle()); }

  SimpleStringDictionary expected_simple_map_;
  AnnotationList expected_annotation_list_;

  DISALLOW_COPY_AND_ASSIGN(ReadFromChildTest);
};

TEST(AnnotationReader, ReadFromChild) {
  ReadFromChildTest test;
  test.Run();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
