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

#include "util/linux/memory_map.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "gtest/gtest.h"
#include "test/file.h"
#include "test/multiprocess.h"
#include "test/scoped_temp_dir.h"
#include "util/file/file_io.h"
#include "util/posix/scoped_mmap.h"

namespace crashpad {
namespace test {
namespace {

TEST(MemoryMap, SelfBasic) {
  ScopedMmap mmapping;
  ASSERT_TRUE(mmapping.ResetMmap(nullptr,
                                 getpagesize(),
                                 PROT_EXEC | PROT_READ,
                                 MAP_SHARED | MAP_ANON,
                                 -1,
                                 0));
  MemoryMap map;
  ASSERT_TRUE(map.Initialize(getpid()));

  auto stack_address = reinterpret_cast<LinuxVMAddress>(&map);
  const MemoryMap::Mapping* mapping = map.FindMapping(stack_address);
  ASSERT_TRUE(mapping);
  EXPECT_GE(stack_address, mapping->start_address);
  EXPECT_LE(stack_address, mapping->end_address);
  EXPECT_TRUE(mapping->readable);
  EXPECT_TRUE(mapping->writable);

  auto code_address = reinterpret_cast<LinuxVMAddress>(&getpid);
  mapping = map.FindMapping(code_address);
  ASSERT_TRUE(mapping);
  EXPECT_GE(code_address, mapping->start_address);
  EXPECT_LE(code_address, mapping->end_address);
  EXPECT_TRUE(mapping->readable);
  EXPECT_FALSE(mapping->writable);
  EXPECT_TRUE(mapping->executable);

  auto mapping_address = mmapping.addr_as<LinuxVMAddress>();
  mapping = map.FindMapping(mapping_address);
  ASSERT_TRUE(mapping);
  mapping = map.FindMapping(mapping_address + mmapping.len() - 1);
  ASSERT_TRUE(mapping);
  EXPECT_EQ(mapping_address, mapping->start_address);
  EXPECT_EQ(mapping_address + mmapping.len(), mapping->end_address);
  EXPECT_TRUE(mapping->readable);
  EXPECT_FALSE(mapping->writable);
  EXPECT_TRUE(mapping->executable);
  EXPECT_TRUE(mapping->shared);
}

class MapParentTest : public Multiprocess {
 public:
  MapParentTest() : Multiprocess(), kFileName("test_file") {}
  ~MapParentTest() {}

 private:
  void MultiprocessParent() override {
    auto code_address = reinterpret_cast<LinuxVMAddress>(getpid);
    CheckedWriteFile(WritePipeHandle(), &code_address, sizeof(code_address));
    auto stack_address = reinterpret_cast<LinuxVMAddress>(&code_address);
    CheckedWriteFile(WritePipeHandle(), &stack_address, sizeof(stack_address));
    ScopedMmap mapping;
    ASSERT_TRUE(mapping.ResetMmap(
        nullptr, getpagesize(), PROT_NONE, MAP_SHARED | MAP_ANON, -1, 0));
    auto mapped_address = mapping.addr_as<LinuxVMAddress>();
    CheckedWriteFile(
        WritePipeHandle(), &mapped_address, sizeof(mapped_address));

    ScopedTempDir temp_dir;
    base::FilePath path = temp_dir.path().Append(FILE_PATH_LITERAL(kFileName));
    ASSERT_FALSE(FileExists(path));
    std::string path_string = path.value();

    ScopedFileHandle handle(
        open(path_string.c_str(), O_CREAT | O_CLOEXEC | O_NOCTTY));

    ScopedMmap file_mapping;
    ASSERT_TRUE(file_mapping.ResetMmap(nullptr,
                                       getpagesize(),
                                       PROT_READ | PROT_WRITE,
                                       MAP_PRIVATE,
                                       handle.get(),
                                       0));
    auto mapped_file_address = file_mapping.addr_as<LinuxVMAddress>();
    CheckedWriteFile(
        WritePipeHandle(), &mapped_file_address, sizeof(mapped_file_address));

    LinuxVMSize path_length = path_string.size();
    CheckedWriteFile(WritePipeHandle(), &path_length, sizeof(path_length));
    CheckedWriteFile(WritePipeHandle(), path_string.c_str(), path_length + 1);

    CheckedReadFileAtEOF(ReadPipeHandle());
  }

  void MultiprocessChild() override {
    LinuxVMAddress code_address;
    CheckedReadFileExactly(
        ReadPipeHandle(), &code_address, sizeof(code_address));
    LinuxVMAddress stack_address;
    CheckedReadFileExactly(
        ReadPipeHandle(), &stack_address, sizeof(stack_address));
    LinuxVMAddress mapped_address;
    CheckedReadFileExactly(
        ReadPipeHandle(), &mapped_address, sizeof(mapped_address));
    LinuxVMAddress mapped_file_address;
    CheckedReadFileExactly(
        ReadPipeHandle(), &mapped_file_address, sizeof(mapped_file_address));
    LinuxVMSize path_length;
    CheckedReadFileExactly(ReadPipeHandle(), &path_length, sizeof(path_length));
    std::unique_ptr<char[]> path_buffer(new char[path_length + 1]);
    CheckedReadFileExactly(
        ReadPipeHandle(), path_buffer.get(), path_length + 1);
    std::string mapped_file_name(path_buffer.get());

    MemoryMap map;
    ASSERT_TRUE(map.Initialize(getppid()));

    const MemoryMap::Mapping* mapping = map.FindMapping(code_address);
    ASSERT_TRUE(mapping);
    EXPECT_GE(code_address, mapping->start_address);
    EXPECT_LE(code_address, mapping->end_address);
    EXPECT_TRUE(mapping->readable);
    EXPECT_TRUE(mapping->executable);

    mapping = map.FindMapping(stack_address);
    ASSERT_TRUE(mapping);
    EXPECT_GE(stack_address, mapping->start_address);
    EXPECT_LE(stack_address, mapping->end_address);
    EXPECT_TRUE(mapping->readable);
    EXPECT_TRUE(mapping->writable);

    mapping = map.FindMapping(mapped_address);
    ASSERT_TRUE(mapping);
    EXPECT_EQ(mapped_address, mapping->start_address);
    EXPECT_EQ(mapped_address + getpagesize(), mapping->end_address);
    EXPECT_FALSE(mapping->readable);
    EXPECT_FALSE(mapping->writable);
    EXPECT_FALSE(mapping->executable);
    EXPECT_TRUE(mapping->shared);

    mapping = map.FindMapping(mapped_file_address);
    ASSERT_TRUE(mapping);
    EXPECT_EQ(mapped_file_address, mapping->start_address);
    EXPECT_TRUE(mapping->readable);
    EXPECT_TRUE(mapping->writable);
    EXPECT_FALSE(mapping->executable);
    EXPECT_FALSE(mapping->shared);
    EXPECT_EQ(mapping->name, mapped_file_name);
  }

  const char* kFileName;
};

TEST(MemoryMap, MapParent) {
  MapParentTest test;
  test.Run();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
