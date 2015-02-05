// Copyright 2014 The Crashpad Authors. All rights reserved.
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

#include "minidump/test/minidump_location_descriptor_list_test_util.h"

#include <windows.h>
#include <dbghelp.h>

#include "minidump/minidump_extensions.h"
#include "minidump/test/minidump_writable_test_util.h"

namespace crashpad {
namespace test {

const MinidumpLocationDescriptorList* MinidumpLocationDescriptorListAtStart(
    const std::string& file_contents, size_t count) {
  MINIDUMP_LOCATION_DESCRIPTOR location_descriptor;
  location_descriptor.DataSize = sizeof(MinidumpLocationDescriptorList) +
                                 count * sizeof(MINIDUMP_LOCATION_DESCRIPTOR);
  location_descriptor.Rva = 0;

  const MinidumpLocationDescriptorList* list =
      MinidumpWritableAtLocationDescriptor<MinidumpLocationDescriptorList>(
          file_contents, location_descriptor);
  if (!list) {
    return nullptr;
  }

  if (list->count != count) {
    EXPECT_EQ(count, list->count);
    return nullptr;
  }

  return list;
}

}  // namespace test
}  // namespace crashpad
