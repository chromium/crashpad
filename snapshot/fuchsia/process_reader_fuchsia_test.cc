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

#include "snapshot/fuchsia/process_reader_fuchsia.h"

#include <zircon/process.h>
#include <zircon/syscalls.h>

#include "gtest/gtest.h"
#include "test/multiprocess_exec.h"

namespace crashpad {
namespace test {
namespace {

TEST(ProcessReaderFuchsia, SelfBasic) {
  ProcessReaderFuchsia process_reader;
  ASSERT_TRUE(process_reader.Initialize(zx_process_self()));

  static constexpr char kTestMemory[] = "Some test memory";
  char buffer[arraysize(kTestMemory)];
  ASSERT_TRUE(process_reader.Memory()->Read(
      reinterpret_cast<zx_vaddr_t>(kTestMemory), sizeof(kTestMemory), &buffer));
  EXPECT_STREQ(kTestMemory, buffer);

  const auto& modules = process_reader.Modules();
  EXPECT_GT(modules.size(), 0u);
  for (const auto& module : modules) {
    EXPECT_FALSE(module.name.empty());
    EXPECT_NE(module.type, ModuleSnapshot::kModuleTypeUnknown);
  }

  const auto& threads = process_reader.Threads();
  EXPECT_GT(threads.size(), 0u);

  zx_info_handle_basic_t info;
  ASSERT_EQ(zx_object_get_info(zx_thread_self(),
                               ZX_INFO_HANDLE_BASIC,
                               &info,
                               sizeof(info),
                               nullptr,
                               nullptr),
            ZX_OK);
  EXPECT_EQ(threads[0].id, info.koid);
  EXPECT_EQ(threads[0].state, ZX_THREAD_STATE_RUNNING);
  EXPECT_EQ(threads[0].name, "initial-thread");
}

constexpr char kTestMemory[] = "Read me from another process";

CRASHPAD_CHILD_TEST_MAIN(ProcessReaderBasicChildTestMain) {
  CheckedReadFileAtEOF(StdioFileHandle(StdioStream::kStandardInput));
  return 0;
}

class BasicChildTest : public MultiprocessExec {
 public:
  BasicChildTest() : MultiprocessExec() {
    SetChildTestMainFunction("ProcessReaderBasicChildTestMain");
  }
  ~BasicChildTest() {}

 private:
  void MultiprocessParent() override {
    ProcessReaderFuchsia process_reader;
    ASSERT_TRUE(process_reader.Initialize(zx_process_self()));

    std::string read_string;
    ASSERT_TRUE(process_reader.Memory()->ReadCString(
        reinterpret_cast<zx_vaddr_t>(kTestMemory), &read_string));
    EXPECT_EQ(read_string, kTestMemory);
  }

  DISALLOW_COPY_AND_ASSIGN(BasicChildTest);
};

TEST(ProcessReaderFuchsia, ChildBasic) {
  BasicChildTest test;
  test.Run();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
