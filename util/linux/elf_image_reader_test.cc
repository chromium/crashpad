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

#include "util/linux/elf_image_reader.h"

#include <dlfcn.h>
#include <unistd.h>

#include "base/logging.h"
#include "gtest/gtest.h"
#include "test/errors.h"
#include "util/linux/address_types.h"
#include "util/misc/from_pointer_cast.h"

namespace crashpad {
namespace test {
namespace {

void foo() {}

TEST(ElfImageReader, MainExecutableSelf) {
  ProcessMemory memory;
  ASSERT_TRUE(memory.Initialize(getpid()));

  Dl_info info;
  ASSERT_TRUE(dladdr(reinterpret_cast<void*>(foo), &info));
  LinuxVMAddress elf_address = FromPointerCast<LinuxVMAddress>(info.dli_fbase);

  ElfImageReader reader;
  ASSERT_TRUE(reader.Initialize(memory, elf_address, "executable"));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
