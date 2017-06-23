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

#include "client/string_annotation.h"

#include <string>
#include <vector>

#include "base/rand_util.h"
#include "client/crashpad_info.h"
#include "gtest/gtest.h"
#include "util/misc/clock.h"
#include "util/thread/thread.h"

namespace crashpad {
namespace test {
namespace {

crashpad::StringAnnotation<8> s_one_{"First"};
crashpad::StringAnnotation<256> s_two_{"Second"};
crashpad::StringAnnotation<101> s_three_{"First"};

class StringAnnotation : public testing::Test {
 public:
  void SetUp() override {
    CrashpadInfo::GetCrashpadInfo()->set_annotations_list(&annotations_);
  }

  void TearDown() override {
    s_one_.Clear();
    s_two_.Clear();
    s_three_.Clear();

    CrashpadInfo::GetCrashpadInfo()->set_annotations_list(nullptr);
  }

 protected:
  using AllAnnotations = std::vector<std::pair<std::string, std::string>>;

  AllAnnotations CollectAnnotations() {
    AllAnnotations annotations;

    internal::Annotation* curr = nullptr;
    while ((curr = annotations_.IteratorNext(curr))) {
      annotations.push_back(std::make_pair(curr->key(), curr->value()));
    }

    return annotations;
  }

  bool ContainsKeyValue(const AllAnnotations& annotations,
                        const std::string& key,
                        const std::string& value) {
    return std::find(annotations.begin(),
                     annotations.end(),
                     std::make_pair(key, value)) != annotations.end();
  }

  AnnotationList annotations_;
};

TEST_F(StringAnnotation, SetAndClear) {
  s_one_.Set("this is a value longer than 8 bytes");
  AllAnnotations annotations = CollectAnnotations();

  EXPECT_EQ(1u, annotations.size());
  EXPECT_TRUE(ContainsKeyValue(annotations, "First", "this is"));

  s_one_.Clear();

  EXPECT_EQ(0u, CollectAnnotations().size());

  s_one_.Set("short");
  s_two_.Set(std::string(500, 'A').data());

  annotations = CollectAnnotations();
  EXPECT_EQ(2u, annotations.size());

  EXPECT_TRUE(ContainsKeyValue(annotations, "First", "short"));
  EXPECT_TRUE(ContainsKeyValue(annotations, "Second", std::string(255, 'A')));
}

TEST_F(StringAnnotation, DuplicateKeys) {
  ASSERT_EQ(0u, CollectAnnotations().size());

  s_one_.Set("1");
  s_three_.Set("2");

  AllAnnotations annotations = CollectAnnotations();
  EXPECT_EQ(2u, annotations.size());

  EXPECT_TRUE(ContainsKeyValue(annotations, "First", "1"));
  EXPECT_TRUE(ContainsKeyValue(annotations, "First", "2"));

  s_one_.Clear();

  annotations = CollectAnnotations();
  EXPECT_EQ(1u, annotations.size());
}

class RaceThread : public Thread {
 private:
  void ThreadMain() override {
    for (int i = 0; i <= 50; ++i) {
      if (i % 2 == 0) {
        s_three_.Set("three");
        s_two_.Clear();
      } else {
        s_three_.Clear();
      }
      SleepNanoseconds(base::RandInt(1, 1000));
    }
  }
};

TEST_F(StringAnnotation, MultipleThreads) {
  ASSERT_EQ(0u, CollectAnnotations().size());

  RaceThread other_thread;
  other_thread.Start();

  for (int i = 0; i <= 50; ++i) {
    if (i % 2 == 0) {
      s_one_.Set("one");
      s_two_.Set("two");
    } else {
      s_one_.Clear();
    }
    SleepNanoseconds(base::RandInt(1, 1000));
  }

  other_thread.Join();

  AllAnnotations annotations = CollectAnnotations();
  EXPECT_GE(annotations.size(), 2u);
  EXPECT_LE(annotations.size(), 3u);

  EXPECT_TRUE(ContainsKeyValue(annotations, "First", "one"));
  EXPECT_TRUE(ContainsKeyValue(annotations, "First", "three"));

  if (annotations.size() == 3) {
    EXPECT_TRUE(ContainsKeyValue(annotations, "Second", "two"));
  }
}

}  // namespace
}  // namespace test
}  // namespace crashpad
