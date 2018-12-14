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

#include "snapshot/fuchsia/memory_map_region_snapshot_fuchsia.h"

#include <dbghelp.h>
#include <zircon/syscalls.h>

#include "base/logging.h"
#include "gtest/gtest.h"
#include "snapshot/fuchsia/process_snapshot_fuchsia.h"
#include "test/multiprocess_exec.h"
#include "util/fuchsia/scoped_task_suspend.h"

namespace crashpad {
namespace test {
namespace {

constexpr struct {
  uint32_t zircon_perm;
  size_t size;
  uint32_t minidump_perm;
} kTestMappingPermAndSizes[] = {
    // Zircon doesn't currently allow write-only, execute-only, or
    // write-execute-only: zx_vmo_create() returns ZX_ERR_ACCESS_DENIED.
    {0, PAGE_SIZE * 5, PAGE_NOACCESS},
    {ZX_VM_PERM_READ, PAGE_SIZE * 6, PAGE_READONLY},
    // {ZX_VM_PERM_WRITE, PAGE_SIZE * 7, PAGE_WRITECOPY},
    // {ZX_VM_PERM_EXECUTE, PAGE_SIZE * 8, PAGE_EXECUTE},
    {ZX_VM_PERM_READ | ZX_VM_PERM_WRITE, PAGE_SIZE * 9, PAGE_READWRITE},
    {ZX_VM_PERM_READ | ZX_VM_PERM_EXECUTE, PAGE_SIZE * 10, PAGE_EXECUTE_READ},
    // {ZX_VM_PERM_WRITE | ZX_VM_PERM_EXECUTE, PAGE_SIZE * 11,
    // PAGE_EXECUTE_WRITECOPY},
    {ZX_VM_PERM_READ | ZX_VM_PERM_WRITE | ZX_VM_PERM_EXECUTE,
     PAGE_SIZE * 12,
     PAGE_EXECUTE_READWRITE},
};

CRASHPAD_CHILD_TEST_MAIN(AddressSpaceChildTestMain) {
  // Create specifically sized mappings/permissions and write the address in
  // our address space to the parent so that the reader can check they're read
  // correctly.
  for (const auto& t : kTestMappingPermAndSizes) {
    zx_handle_t vmo = ZX_HANDLE_INVALID;
    CHECK(zx_vmo_create(t.size, 0, &vmo) == ZX_OK);
    uintptr_t mapping_addr = 0;
    CHECK(zx_vmar_map(zx_vmar_root_self(),
                      t.zircon_perm,
                      0,
                      vmo,
                      0,
                      t.size,
                      &mapping_addr) == ZX_OK);
    CheckedWriteFile(StdioFileHandle(StdioStream::kStandardOutput),
                     &mapping_addr,
                     sizeof(mapping_addr));
  }

  CheckedReadFileAtEOF(StdioFileHandle(StdioStream::kStandardInput));
  return 0;
}

bool FindMappingMatching(
    const std::vector<const MemoryMapRegionSnapshot*>& memory_map,
    uintptr_t address,
    size_t size,
    uint32_t perm) {
  for (const auto* region : memory_map) {
    const MINIDUMP_MEMORY_INFO& mmi = region->AsMinidumpMemoryInfo();
    if (mmi.BaseAddress == address && mmi.RegionSize == size &&
        mmi.Protect == perm) {
      return true;
    }
  }

  return false;
}

class AddressSpaceTest : public MultiprocessExec {
 public:
  AddressSpaceTest() : MultiprocessExec() {
    SetChildTestMainFunction("AddressSpaceChildTestMain");
  }
  ~AddressSpaceTest() {}

 private:
  void MultiprocessParent() override {
    uintptr_t test_addresses[arraysize(kTestMappingPermAndSizes)];
    for (size_t i = 0; i < arraysize(test_addresses); ++i) {
      ASSERT_TRUE(ReadFileExactly(
          ReadPipeHandle(), &test_addresses[i], sizeof(test_addresses[i])));
    }

    ScopedTaskSuspend suspend(*ChildProcess());

    ProcessSnapshotFuchsia process_snapshot;
    ASSERT_TRUE(process_snapshot.Initialize(*ChildProcess()));

    for (size_t i = 0; i < arraysize(test_addresses); ++i) {
      EXPECT_TRUE(
          FindMappingMatching(process_snapshot.MemoryMap(),
                              test_addresses[i],
                              kTestMappingPermAndSizes[i].size,
                              kTestMappingPermAndSizes[i].minidump_perm));
    }
  }

  DISALLOW_COPY_AND_ASSIGN(AddressSpaceTest);
};

TEST(ProcessSnapshotFuchsiaTest, AddressSpaceMapping) {
  AddressSpaceTest test;
  test.Run();
}


}  // namespace
}  // namespace test
}  // namespace crashpad
