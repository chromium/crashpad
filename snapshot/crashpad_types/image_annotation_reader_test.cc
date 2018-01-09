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

#include "snapshot/crashpad_types/image_annotation_reader.h"

#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>

#include "base/logging.h"
#include "build/build_config.h"
#include "client/annotation.h"
#include "client/annotation_list.h"
#include "client/simple_string_dictionary.h"
#include "gtest/gtest.h"
#include "test/multiprocess.h"
#include "util/file/file_io.h"
#include "util/misc/as_underlying_type.h"
#include "util/misc/from_pointer_cast.h"
#include "util/process/process_memory_linux.h"

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
    EXPECT_EQ(annotation.value.size(), expected_annotation->size());
    EXPECT_EQ(memcmp(annotation.value.data(),
                     expected_annotation->value(),
                     std::min(VMSize{annotation.value.size()},
                              VMSize{expected_annotation->size()})),
              0);
  }
}

class AnnotationTest {
 public:
  AnnotationTest()
      : expected_simple_map_(),
        test_annotations_(),
        expected_annotation_list_() {
    expected_simple_map_.SetKeyValue("key", "value");
    expected_simple_map_.SetKeyValue("key2", "value2");

    static constexpr char kAnnotationName[] = "test annotation";
    static constexpr char kAnnotationValue[] = "test annotation value";
    test_annotations_.push_back(std::make_unique<Annotation>(
        Annotation::Type::kString,
        kAnnotationName,
        reinterpret_cast<void*>(const_cast<char*>(kAnnotationValue))));
    test_annotations_.back()->SetSize(sizeof(kAnnotationValue));
    expected_annotation_list_.Add(test_annotations_.back().get());

    static constexpr char kAnnotationName2[] = "test annotation2";
    static constexpr char kAnnotationValue2[] = "test annotation value2";
    test_annotations_.push_back(std::make_unique<Annotation>(
        Annotation::Type::kString,
        kAnnotationName2,
        reinterpret_cast<void*>(const_cast<char*>(kAnnotationValue2))));
    test_annotations_.back()->SetSize(sizeof(kAnnotationValue2));
    expected_annotation_list_.Add(test_annotations_.back().get());
  }

  ~AnnotationTest() = default;

  void ExpectAnnotations(pid_t pid, bool is_64_bit) {
    ProcessMemoryLinux memory;
    ASSERT_TRUE(memory.Initialize(pid));

    ProcessMemoryRange range;
    ASSERT_TRUE(range.Initialize(&memory, is_64_bit));

    ImageAnnotationReader reader(&range);

    std::map<std::string, std::string> simple_map;
    ASSERT_TRUE(reader.SimpleMap(
        FromPointerCast<VMAddress>(&expected_simple_map_), &simple_map));
    ExpectSimpleMap(simple_map, expected_simple_map_);

    std::vector<AnnotationSnapshot> annotation_list;
    ASSERT_TRUE(reader.AnnotationsList(
        FromPointerCast<VMAddress>(&expected_annotation_list_),
        &annotation_list));
    ExpectAnnotationList(annotation_list, expected_annotation_list_);
  }

 private:
  SimpleStringDictionary expected_simple_map_;
  std::vector<std::unique_ptr<Annotation>> test_annotations_;
  AnnotationList expected_annotation_list_;

  DISALLOW_COPY_AND_ASSIGN(AnnotationTest);
};

TEST(ImageAnnotationReader, ReadFromSelf) {
  AnnotationTest test;

#if defined(ARCH_CPU_64_BITS)
  constexpr bool am_64_bit = true;
#else
  constexpr bool am_64_bit = false;
#endif

  test.ExpectAnnotations(getpid(), am_64_bit);
}

class ReadFromChildTest : public Multiprocess {
 public:
  ReadFromChildTest() : Multiprocess(), annotation_test_() {}

  ~ReadFromChildTest() {}

 private:
  void MultiprocessParent() {
#if defined(ARCH_CPU_64_BITS)
    constexpr bool am_64_bit = true;
#else
    constexpr bool am_64_bit = false;
#endif
    annotation_test_.ExpectAnnotations(ChildPID(), am_64_bit);
  }

  void MultiprocessChild() { CheckedReadFileAtEOF(ReadPipeHandle()); }

  AnnotationTest annotation_test_;

  DISALLOW_COPY_AND_ASSIGN(ReadFromChildTest);
};

TEST(ImageAnnotationReader, ReadFromChild) {
  ReadFromChildTest test;
  test.Run();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
