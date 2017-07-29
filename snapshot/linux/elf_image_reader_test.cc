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

#include "snapshot/linux/elf_image_reader.h"

#include <dlfcn.h>
#include <unistd.h>

#include "base/logging.h"
#include "build/build_config.h"
#include "gtest/gtest.h"
#include "test/multiprocess.h"
#include "util/file/file_io.h"
#include "util/linux/address_types.h"
#include "util/linux/auxiliary_vector.h"
#include "util/linux/memory_map.h"
#include "util/misc/from_pointer_cast.h"

extern "C" {
__attribute__((visibility("default"))) void
ElfImageReaderTestExportedSymbol(){};
}  // extern "C"

namespace crashpad {
namespace test {
namespace {

void LocateExecutable(pid_t pid, bool is_64_bit, LinuxVMAddress* elf_address) {
  AuxiliaryVector aux;
  ASSERT_TRUE(aux.Initialize(pid, is_64_bit));

  LinuxVMAddress phdrs;
  ASSERT_TRUE(aux.GetValue(AT_PHDR, &phdrs));

  MemoryMap memory_map;
  ASSERT_TRUE(memory_map.Initialize(pid));
  const MemoryMap::Mapping* phdr_mapping = memory_map.FindMapping(phdrs);
  ASSERT_TRUE(phdr_mapping);
  const MemoryMap::Mapping* exe_mapping =
      memory_map.FindFileMmapStart(*phdr_mapping);
  ASSERT_TRUE(exe_mapping);
  *elf_address = exe_mapping->range.Base();
}

void ExpectElfImageWithSymbol(pid_t pid,
                              LinuxVMAddress address,
                              bool is_64_bit,
                              std::string symbol_name,
                              LinuxVMAddress expected_symbol_address) {
  ProcessMemory memory;
  ASSERT_TRUE(memory.Initialize(pid));
  ProcessMemoryRange range;
  ASSERT_TRUE(range.Initialize(&memory, is_64_bit));

  ElfImageReader reader;
  ASSERT_TRUE(reader.Initialize(range, address));

  LinuxVMAddress symbol_address;
  LinuxVMSize symbol_size;
  ASSERT_TRUE(
      reader.GetDynamicSymbol(symbol_name, &symbol_address, &symbol_size));
  EXPECT_EQ(symbol_address, expected_symbol_address);

  EXPECT_FALSE(
      reader.GetDynamicSymbol("notasymbol", &symbol_address, &symbol_size));
}

void ReadThisExecutableInTarget(pid_t pid) {
#if defined(ARCH_CPU_64_BITS)
  constexpr bool am_64_bit = true;
#else
  constexpr bool am_64_bit = false;
#endif  // ARCH_CPU_64_BITS

  LinuxVMAddress elf_address;
  LocateExecutable(pid, am_64_bit, &elf_address);

  ExpectElfImageWithSymbol(
      pid,
      elf_address,
      am_64_bit,
      "ElfImageReaderTestExportedSymbol",
      FromPointerCast<LinuxVMAddress>(ElfImageReaderTestExportedSymbol));
}

// Assumes that libc is loaded at the same address in this process as in the
// target, which it is for the fork test below.
void ReadLibcInTarget(pid_t pid) {
#if defined(ARCH_CPU_64_BITS)
  constexpr bool am_64_bit = true;
#else
  constexpr bool am_64_bit = false;
#endif  // ARCH_CPU_64_BITS

  Dl_info info;
  ASSERT_TRUE(dladdr(reinterpret_cast<void*>(getpid), &info)) << "dladdr:"
                                                              << dlerror();
  LinuxVMAddress elf_address = FromPointerCast<LinuxVMAddress>(info.dli_fbase);

  ExpectElfImageWithSymbol(pid,
                           elf_address,
                           am_64_bit,
                           "getpid",
                           FromPointerCast<LinuxVMAddress>(getpid));
}

class ReadExecutableChildTest : public Multiprocess {
 public:
  ReadExecutableChildTest() : Multiprocess() {}
  ~ReadExecutableChildTest() {}

 private:
  void MultiprocessParent() { ReadThisExecutableInTarget(ChildPID()); }
  void MultiprocessChild() { CheckedReadFileAtEOF(ReadPipeHandle()); }
};

TEST(ElfImageReader, MainExecutableSelf) {
  ReadThisExecutableInTarget(getpid());
}

TEST(ElfImageReader, MainExecutableChild) {
  ReadExecutableChildTest test;
  test.Run();
}

TEST(ElfImageReader, OneModuleSelf) {
  ReadLibcInTarget(getpid());
}

class ReadLibcChildTest : public Multiprocess {
 public:
  ReadLibcChildTest() : Multiprocess() {}
  ~ReadLibcChildTest() {}

 private:
  void MultiprocessParent() { ReadLibcInTarget(ChildPID()); }
  void MultiprocessChild() { CheckedReadFileAtEOF(ReadPipeHandle()); }
};

TEST(ElfImageReader, OneModuleChild) {
  ReadLibcChildTest test;
  test.Run();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
