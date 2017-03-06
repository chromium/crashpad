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

#include "client/annotations.h"

#include "gtest/gtest.h"
#include "snapshot/annotations_shared_header_test.h"
#include "snapshot/annotations_test_helper.h"

namespace crashpad {
namespace test {
namespace {

TEST(Annotations, File1) {
  std::map<std::string, std::string> initial = GetAllAnnotations();
  ASSERT_FALSE(HasFailure());

  CRASHPAD_SET_ANNOTATION_STRING_CONSTANT(cross_file_annotation, "mystuff");

  std::map<std::string, std::string> current = GetAllAnnotations();
  ASSERT_FALSE(HasFailure());

  EXPECT_EQ("mystuff", current["cross_file_annotation"]);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
