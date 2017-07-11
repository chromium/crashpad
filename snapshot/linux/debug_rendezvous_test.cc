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

#include "snapshot/linux/debug_rendezvous.h"

#include <link.h>
#include <sys/types.h>

#include "gtest/gtest.h"
#include "snapshot/linux/elf_image_reader.h"
#include "test/multiprocess.h"
#include "util/linux/address_types.h"
#include "util/linux/auxiliary_vector.h"
#include "util/linux/memory_map.h"
#include "util/linux/process_memory.h"
#include "util/linux/process_memory_range.h"
#include "util/misc/from_pointer_cast.h"

namespace crashpad {
namespace test {
namespace {

void TestAgainstTarget(pid_t pid, bool is_64_bit) {
  // Use ElfImageReader on the main executable which can tell us the debug
  // address. GNU declares the symbol _r_debug in link.h which we can use to get
  // the address, but Android does not.
  AuxiliaryVector aux;
  ASSERT_TRUE(aux.Initialize(pid, is_64_bit));

  LinuxVMAddress phdrs;
  ASSERT_TRUE(aux.GetValue(AT_PHDR, &phdrs));

  MemoryMap mappings;
  ASSERT_TRUE(mappings.Initialize(pid));

  const MemoryMap::Mapping* phdr_mapping = mappings.FindMapping(phdrs);
  ASSERT_TRUE(phdr_mapping);
  const MemoryMap::Mapping* exe_mapping = mappings.FindFileMmapStart(phdr_mapping);
  LinuxVMAddress elf_address = exe_mapping->range.Base();

  ProcessMemory memory;
  ASSERT_TRUE(memory.Initialize(pid));
  ProcessMemoryRange range;
  ASSERT_TRUE(range.Initialize(&memory, is_64_bit));

  ElfImageReader exe_reader;
  ASSERT_TRUE(exe_reader.Initialize(range, elf_address));
  LinuxVMAddress debug_address;
  ASSERT_TRUE(exe_reader.GetDebugAddress(&debug_address));

  // start the actual tests
  DebugRendezvous debug;
  ASSERT_TRUE(debug.Initialize(memory, debug_address, is_64_bit));

  EXPECT_EQ(debug.Executable()->load_bias, exe_reader.GetLoadBias());

#if defined(OS_ANDROID)
  EXPECT_NE(debug.Executable()->name.find("crashpad_snapshot_test"),
            std::string::npos);

  // Android's dynamic linker does not set the dynamic array for the executable.
  EXPECT_EQ(debug.Executable()->dynamic_array, 0u);
#else
  // GNU's dynamic linker does not set the name for the executable.
  EXPECT_TRUE(debug.Executable()->name.empty());

  CheckedLinuxAddressRange exe_range(
      is_64_bit, exe_reader.Address(), exe_reader.Size());
  EXPECT_TRUE(exe_range.ContainsValue(debug.Executable()->dynamic_array));
#endif  // OS_ANDROID

  for (const DebugRendezvous::LinkEntry& module : debug.Modules()) {
    const MemoryMap::Mapping* mapping =
        mappings.FindMapping(module.dynamic_array);
    ASSERT_TRUE(mapping);

    const MemoryMap::Mapping* module_mapping =
        mappings.FindFileMmapStart(*mapping);
    ASSERT_TRUE(module_mapping);

#if !defined(OS_ANDROID)
    // GNU's dynamic linker doesn't set the name in the link map for the vdso.
    if (module_mapping->name != "[vdso]") {
      EXPECT_FALSE(module.name.empty());
    }
#endif

    ElfImageReader module_reader;
    ASSERT_TRUE(module_reader.Initialize(range, module_mapping->range.Base()));
    EXPECT_EQ(module.load_bias, module_reader.GetLoadBias());
    CheckedLinuxAddressRange module_range(
        is_64_bit, module_reader.Address(), module_reader.Size());
    EXPECT_TRUE(module_range.ContainsValue(module.dynamic_array));
  }
}

TEST(DebugRendezvous, Self) {
#if defined(ARCH_CPU_64_BITS)
  constexpr bool am_64_bit = true;
#else
  constexpr bool am_64_bit = false;
#endif

  TestAgainstTarget(getpid(), am_64_bit);
}

class ChildTest : public Multiprocess {
 public:
  ChildTest() {}
  ~ChildTest() {}

 private:
  void MultiprocessParent() {
#if defined(ARCH_CPU_64_BITS)
    constexpr bool am_64_bit = true;
#else
    constexpr bool am_64_bit = false;
#endif

    TestAgainstTarget(ChildPID(), am_64_bit);
  }

  void MultiprocessChild() { CheckedReadFileAtEOF(ReadPipeHandle()); }

  DISALLOW_COPY_AND_ASSIGN(ChildTest);
};

TEST(DebugRendezvous, Child) {
  ChildTest test;
  test.Run();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
