// Copyright 2021 The Crashpad Authors. All rights reserved.
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

#include "client/ios_handler/in_process_intermediate_dump_handler.h"

#include "base/files/file_path.h"
#include "gtest/gtest.h"
#include "test/scoped_temp_dir.h"
#include "test/test_paths.h"
#include "util/file/filesystem.h"
#include "util/misc/capture_context.h"

namespace crashpad {
namespace test {
namespace {

using internal::InProcessIntermediateDumpHandler;

class InProcessIntermediateDumpHandlerTest : public testing::Test {
 protected:
  // testing::Test:

  void SetUp() override {
    path_ = temp_dir_.path().Append("dump_file");
    writer_ = std::make_unique<internal::IOSIntermediateDumpWriter>();
    EXPECT_TRUE(writer_->Open(path_));
    ASSERT_TRUE(IsRegularFile(path_));
  }

  void TearDown() override {
    writer_.reset();
    EXPECT_FALSE(IsRegularFile(path_));
  }

  const auto& path() const { return path_; }
  auto writer() const { return writer_.get(); }

 private:
  std::unique_ptr<internal::IOSIntermediateDumpWriter> writer_;
  ScopedTempDir temp_dir_;
  base::FilePath path_;
};

TEST_F(InProcessIntermediateDumpHandlerTest, Default) {
  internal::IOSSystemDataCollector system_data;
  {
    internal::IOSIntermediateDumpWriter::ScopedRootMap rootMap(writer());
    InProcessIntermediateDumpHandler::WriteHeader(writer());
    InProcessIntermediateDumpHandler::WriteProcessInfo(writer());
    InProcessIntermediateDumpHandler::WriteSystemInfo(writer(), system_data);
    InProcessIntermediateDumpHandler::WriteThreadInfo(writer(), 0, 0);
    InProcessIntermediateDumpHandler::WriteModuleInfo(writer());

    crashpad::NativeCPUContext cpu_context;
    crashpad::CaptureContext(&cpu_context);
    const mach_exception_data_type_t code[2] = {};
    static constexpr int kSimulatedException = -1;
    InProcessIntermediateDumpHandler::WriteMachExceptionInfo(
        writer(),
        MACH_EXCEPTION_CODES,
        mach_thread_self(),
        kSimulatedException,
        code,
        base::size(code),
        MACHINE_THREAD_STATE,
        reinterpret_cast<ConstThreadState>(&cpu_context),
        MACHINE_THREAD_STATE_COUNT);
  }
}

}  // namespace
}  // namespace test
}  // namespace crashpad
