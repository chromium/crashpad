// Copyright 2015 The Crashpad Authors. All rights reserved.
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

#include "test/win/win_multiprocess.h"

#include <shellapi.h>
#include <stdlib.h>

#include "base/logging.h"
#include "base/scoped_generic.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "gtest/gtest.h"
#include "util/stdlib/string_number_conversion.h"
#include "util/string/split_string.h"
#include "test/paths.h"

namespace crashpad {
namespace test {

namespace {

const char kIsMultiprocessChild[] = "--is-multiprocess-child";

struct LocalFreeTraits {
  static HLOCAL InvalidValue() { return nullptr; }
  static void Free(HLOCAL mem) {
    if (LocalFree(mem) != nullptr)
      PLOG(ERROR) << "LocalFree";
  }
};

using ScopedLocalFree = base::ScopedGeneric<HLOCAL, LocalFreeTraits>;

bool GetSwitch(const char* switch_name, std::string* value) {
  int num_args;
  wchar_t** args = CommandLineToArgvW(GetCommandLine(), &num_args);
  ScopedLocalFree scoped_args(args);  // Take ownership.
  if (!args) {
    PLOG(ERROR) << "couldn't parse command line";
    return false;
  }

  std::string switch_name_with_equals(switch_name);
  switch_name_with_equals += "=";
  for (size_t i = 1; i < num_args; ++i) {
    const wchar_t* arg = args[i];
    std::string arg_as_utf8 = base::UTF16ToUTF8(arg);
    if (arg_as_utf8.compare(
            0, switch_name_with_equals.size(), switch_name_with_equals) == 0) {
      *value = arg_as_utf8.substr(switch_name_with_equals.size());
      return true;
    }
  }

  return false;
}

}  // namespace

WinMultiprocess::WinMultiprocess()
    : pipe_c2p_read_(),
      pipe_c2p_write_(),
      pipe_p2c_read_(),
      pipe_p2c_write_(),
      child_handle_(),
      exit_code_(EXIT_SUCCESS) {
}

void WinMultiprocess::Run() {
  std::string switch_value;
  if (GetSwitch(kIsMultiprocessChild, &switch_value)) {
    // If we're in the child, then set up the handles we inherited from the
    // parent. These are inherited from the parent and so are open and have the
    // same value as in the parent. The values are passed to the child on the
    // command line.
    std::string left, right;
    ASSERT_TRUE(SplitString(switch_value, '|', &left, &right));
    unsigned int c2p_write, p2c_read;
    ASSERT_TRUE(StringToNumber(left, &c2p_write));
    ASSERT_TRUE(StringToNumber(right, &p2c_read));
    pipe_c2p_write_.reset(reinterpret_cast<HANDLE>(c2p_write));
    pipe_p2c_read_.reset(reinterpret_cast<HANDLE>(p2c_read));

    // Notify the parent that it's OK to proceed. We only need to wait to get to
    // the process entry point, but this is the easiest place we can notify.
    char c = ' ';
    CheckedWriteFile(WritePipeHandle(), &c, sizeof(c));

    // Invoke the child side of the test.
    WinMultiprocessChild();
    exit(0);
  } else {
    // If we're in the parent, make pipes for child-to-parent and
    // parent-to-child communication. Mark them as inheritable via the
    // SECURITY_ATTRIBUTES, but use SetHandleInformation to ensure that the
    // parent sides are not inherited.
    SECURITY_ATTRIBUTES security_attributes = {0};
    security_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    security_attributes.bInheritHandle = true;

    HANDLE c2p_read, c2p_write;
    PCHECK(CreatePipe(&c2p_read, &c2p_write, &security_attributes, 0));
    PCHECK(SetHandleInformation(c2p_read, HANDLE_FLAG_INHERIT, 0));
    pipe_c2p_read_.reset(c2p_read);
    pipe_c2p_write_.reset(c2p_write);

    HANDLE p2c_read, p2c_write;
    PCHECK(CreatePipe(&p2c_read, &p2c_write, &security_attributes, 0));
    PCHECK(SetHandleInformation(p2c_write, HANDLE_FLAG_INHERIT, 0));
    pipe_p2c_read_.reset(p2c_read);
    pipe_p2c_write_.reset(p2c_write);

    // Build a command line for the child process that tells it only to run the
    // current test, and to pass down the values of the pipe handles.
    const ::testing::TestInfo* const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    std::wstring command_line = Paths::Executable().value() + L" " +
                                base::UTF8ToUTF16(base::StringPrintf(
                                    "--gtest_filter=%s.%s %s=0x%x|0x%x",
                                    test_info->test_case_name(),
                                    test_info->name(),
                                    kIsMultiprocessChild,
                                    c2p_write,
                                    p2c_read));
    STARTUPINFO startup_info = {0};
    startup_info.cb = sizeof(startup_info);
    startup_info.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup_info.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startup_info.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    startup_info.dwFlags = STARTF_USESTDHANDLES;
    PROCESS_INFORMATION process_info;
    PCHECK(
        CreateProcess(Paths::Executable().value().c_str(),
                      &command_line[0],  // This cannot be constant, per MSDN.
                      nullptr,
                      nullptr,
                      true,  // Inherit handles.
                      0,
                      nullptr,
                      nullptr,
                      &startup_info,
                      &process_info));
    child_handle_.reset(process_info.hProcess);
    CloseHandle(process_info.hThread);

    // Block until the child process has launched. CreateProcess() returns
    // immediately, and test code expects process initialization to have
    // completed so it can, for example, use the process handle.
    char c;
    CheckedReadFile(pipe_c2p_read_.get(), &c, sizeof(c));
    ASSERT_EQ(' ', c);

    // These have been passed to the child, close our side.
    pipe_c2p_write_.reset();
    pipe_p2c_read_.reset();

    WinMultiprocessParent();

    // Close our side of the handles now that we're done. The child can
    // use this to know when it's safe to complete.
    pipe_p2c_write_.reset();
    pipe_c2p_read_.reset();

    // Wait for the child to complete.
    ASSERT_EQ(WAIT_OBJECT_0,
              WaitForSingleObject(child_handle_.get(), INFINITE));

    DWORD exit_code;
    ASSERT_TRUE(GetExitCodeProcess(child_handle_.get(), &exit_code));
    ASSERT_EQ(exit_code_, exit_code);
  }
}

void WinMultiprocess::SetExpectedChildExitCode(unsigned int exit_code) {
  exit_code_ = exit_code;
}

WinMultiprocess::~WinMultiprocess() {
}

FileHandle WinMultiprocess::ReadPipeHandle() const {
  FileHandle handle =
      child_handle_.get() ? pipe_c2p_read_.get() : pipe_p2c_read_.get();
  CHECK(handle != nullptr);
  return handle;
}

FileHandle WinMultiprocess::WritePipeHandle() const {
  FileHandle handle =
      child_handle_.get() ? pipe_p2c_write_.get() : pipe_c2p_write_.get();
  CHECK(handle != nullptr);
  return handle;
}

void WinMultiprocess::CloseReadPipe() {
  if (child_handle_.get())
    pipe_c2p_read_.reset();
  else
    pipe_p2c_read_.reset();
}

void WinMultiprocess::CloseWritePipe() {
  if (child_handle_.get())
    pipe_p2c_write_.reset();
  else
    pipe_c2p_write_.reset();
}

HANDLE WinMultiprocess::ChildProcess() const {
  EXPECT_NE(nullptr, child_handle_.get());
  return child_handle_.get();
}

}  // namespace test
}  // namespace crashpad
