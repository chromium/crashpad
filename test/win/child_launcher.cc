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

#include "test/win/child_launcher.h"

#include "gtest/gtest.h"

namespace crashpad {
namespace test {

ChildLauncher::ChildLauncher(const std::wstring& executable,
                             const std::wstring& command_line)
    : executable_(executable),
      command_line_(command_line),
      process_handle_(),
      main_thread_handle_(),
      stdout_read_handle_() {
}

ChildLauncher::~ChildLauncher() {
  EXPECT_EQ(WAIT_OBJECT_0,
            WaitForSingleObject(process_handle_.get(), INFINITE));
}

void ChildLauncher::Start() {
  ASSERT_FALSE(process_handle_.is_valid());
  ASSERT_FALSE(main_thread_handle_.is_valid());
  ASSERT_FALSE(stdout_read_handle_.is_valid());

  // Create a pipe for the stdout of the child.
  SECURITY_ATTRIBUTES security_attributes = {0};
  security_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
  security_attributes.bInheritHandle = true;
  HANDLE stdout_read;
  HANDLE stdout_write;
  ASSERT_TRUE(CreatePipe(&stdout_read, &stdout_write, &security_attributes, 0));
  stdout_read_handle_.reset(stdout_read);
  ScopedFileHANDLE write_handle(stdout_write);
  ASSERT_TRUE(
      SetHandleInformation(stdout_read_handle_.get(), HANDLE_FLAG_INHERIT, 0));

  STARTUPINFO startup_info = {0};
  startup_info.cb = sizeof(startup_info);
  startup_info.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  startup_info.hStdOutput = write_handle.get();
  startup_info.hStdError = GetStdHandle(STD_ERROR_HANDLE);
  startup_info.dwFlags = STARTF_USESTDHANDLES;
  PROCESS_INFORMATION process_information;
  std::wstring command_line;
  AppendCommandLineArgument(executable_, &command_line);
  command_line += L" ";
  command_line += command_line_;
  ASSERT_TRUE(CreateProcess(executable_.c_str(),
                            &command_line[0],
                            nullptr,
                            nullptr,
                            true,
                            0,
                            nullptr,
                            nullptr,
                            &startup_info,
                            &process_information));
  // Take ownership of the two process handles returned.
  main_thread_handle_.reset(process_information.hThread);
  process_handle_.reset(process_information.hProcess);
}

// Ref: http://blogs.msdn.com/b/twistylittlepassagesallalike/archive/2011/04/23/everyone-quotes-arguments-the-wrong-way.aspx
void AppendCommandLineArgument(const std::wstring& argument,
                               std::wstring* command_line) {
  // Don't bother quoting if unnecessary.
  if (!argument.empty() &&
      argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
    command_line->append(argument);
  } else {
    command_line->push_back(L'"');
    for (std::wstring::const_iterator i = argument.begin();; ++i) {
      size_t backslash_count = 0;
      while (i != argument.end() && *i == L'\\') {
        ++i;
        ++backslash_count;
      }
      if (i == argument.end()) {
        // Escape all backslashes, but let the terminating double quotation mark
        // we add below be interpreted as a metacharacter.
        command_line->append(backslash_count * 2, L'\\');
        break;
      } else if (*i == L'"') {
        // Escape all backslashes and the following double quotation mark.
        command_line->append(backslash_count * 2 + 1, L'\\');
        command_line->push_back(*i);
      } else {
        // Backslashes aren't special here.
        command_line->append(backslash_count, L'\\');
        command_line->push_back(*i);
      }
    }
    command_line->push_back(L'"');
  }
}

}  // namespace test
}  // namespace crashpad
