// Copyright 2016 The Crashpad Authors. All rights reserved.
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

#include "base/logging.h"
#include "gtest/gtest.h"

CRASHPAD_DEFINE_ANNOTATION_STRING_CONSTANT(test_annotation_0);
CRASHPAD_DEFINE_ANNOTATION_STRING_CONSTANT(test_annotation_1);
    
namespace crashpad {
namespace test {
namespace {

TEST(Annotations, Basic) {
  CRASHPAD_SET_ANNOTATION_STRING_CONSTANT(test_annotation_0, "things!");
  CRASHPAD_SET_ANNOTATION_STRING_CONSTANT(test_annotation_1, "stuff");

  CRASHPAD_SET_ANNOTATION_STRING_CONSTANT(test_annotation_0, "different");
}

}  // namespace
}  // namespace test
}  // namespace crashpad
