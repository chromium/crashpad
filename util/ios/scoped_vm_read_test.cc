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

#include "util/ios/scoped_vm_read.h"

#include "gtest/gtest.h"

namespace crashpad {
namespace test {
namespace {

TEST(ScopedVMReadTest, BasicFunctionality) {
  internal::ScopedVMRead<void> vmread_null(nullptr, 0);
  ASSERT_FALSE(vmread_null.is_valid());

  internal::ScopedVMRead<void> vmread_bad(0x001, 100);
  ASSERT_FALSE(vmread_bad.is_valid());

  // Test reading struct
  //  ScopedVMRead<process_types::AnnotationList> annotation_list(
  //      crashpad_info->annotations_list);
  //  ASSERT_TRUE(annotation_list.is_valid());
  //
  //  // Test reading array
  //  ScopedVMRead<SimpleStringDictionary> simple_annotations(
  //      crashpad_info->simple_annotations,
  //      sizeof(SimpleStringDictionary::Entry) *
  //          SimpleStringDictionary::num_entries);
  //  ASSERT_TRUE(simple_annotations.is_valid());
}

}  // namespace
}  // namespace test
}  // namespace crashpad
