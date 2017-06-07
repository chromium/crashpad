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
#include "test/multiprocess.h"
#include "util/file/file_io.h"
#include "util/linux/address_types.h"
#include "util/misc/from_pointer_cast.h"

extern "C" {
__attribute__((visibility("default"))) void foo() {}
}

namespace crashpad {
namespace test {
namespace {

void ReadElfWithSymbolForCloneOrSelf(pid_t pid, std::string name, void* addr) {
  ProcessMemory memory;
  ASSERT_TRUE(memory.Initialize(pid));

  Dl_info info;
  ASSERT_TRUE(dladdr(addr, &info)) << "dladdr:" << dlerror();
  LinuxVMAddress elf_address = FromPointerCast<LinuxVMAddress>(info.dli_fbase);

  ElfImageReader reader;
  ASSERT_TRUE(reader.Initialize(&memory, elf_address));

#if defined(ARCH_CPU_64_BITS)
  EXPECT_TRUE(reader.Is64Bit());
#else
  EXPECT_FALSE(reader.Is64Bit());
#endif

  LinuxVMAddress symbol_addr;
  LinuxVMSize symbol_size;
  ASSERT_TRUE(reader.GetSymbol(name, &symbol_addr, &symbol_size));
  EXPECT_EQ(symbol_addr, FromPointerCast<LinuxVMAddress>(addr));

  EXPECT_FALSE(reader.GetSymbol("notasymbol", &symbol_addr, &symbol_size));
}

void ReadExecutableForTarget(pid_t pid) {
  ReadElfWithSymbolForCloneOrSelf(pid, "foo", reinterpret_cast<void*>(foo));
}

void ReadLibcForTarget(pid_t pid) {
  ReadElfWithSymbolForCloneOrSelf(pid, "getpid", reinterpret_cast<void*>(getpid));
}

class ReadExecutableChildTest : public Multiprocess {
 public:
  ReadExecutableChildTest() : Multiprocess() {}
  ~ReadExecutableChildTest() {}

 private:
  void MultiprocessParent() { ReadExecutableForTarget(ChildPID()); }
  void MultiprocessChild() { CheckedReadFileAtEOF(ReadPipeHandle()); }
};

TEST(ElfImageReader, MainExecutableSelf) {
  ReadExecutableForTarget(getpid());
}

TEST(ElfImageReader, MainExecutableChild) {
  ReadExecutableChildTest test;
  test.Run();
}

TEST(ElfImageReader, OneModuleSelf) {
  ReadLibcForTarget(getpid());
}

class ReadLibcChildTest : public Multiprocess {
 public:
  ReadLibcChildTest() : Multiprocess() {}
  ~ReadLibcChildTest() {}

 private:
  void MultiprocessParent() { ReadLibcForTarget(ChildPID()); }
  void MultiprocessChild() { CheckedReadFileAtEOF(ReadPipeHandle()); }
};

TEST(ElfImageReader, OneModuleChild) {
  ReadLibcChildTest test;
  test.Run();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
