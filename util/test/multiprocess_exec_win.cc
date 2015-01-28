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

#include "util/test/multiprocess_exec.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "gtest/gtest.h"

namespace crashpad {
namespace test {

namespace {

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

}  // namespace

namespace internal {

struct MultiprocessInfo {
  MultiprocessInfo() {}
  ScopedFileHANDLE pipe_c2p_read;
  ScopedFileHANDLE pipe_c2p_write;
  ScopedFileHANDLE pipe_p2c_read;
  ScopedFileHANDLE pipe_p2c_write;
  PROCESS_INFORMATION process_info;
};

}  // namespace internal

Multiprocess::Multiprocess()
    : info_(nullptr),
      code_(EXIT_SUCCESS),
      reason_(kTerminationNormal) {
}

void Multiprocess::Run() {
  // Set up and spawn the child process.
  ASSERT_NO_FATAL_FAILURE(PreFork());
  RunChild();

  // And then run the parent actions in this process.
  RunParent();

  // Reap the child.
  WaitForSingleObject(info_->process_info.hProcess, INFINITE);
  CloseHandle(info_->process_info.hThread);
  CloseHandle(info_->process_info.hProcess);
}

Multiprocess::~Multiprocess() {
  delete info_;
}

void Multiprocess::PreFork() {
  NOTREACHED();
}

FileHandle Multiprocess::ReadPipeHandle() const {
  // This is the parent case, it's stdin in the child.
  return info_->pipe_c2p_read.get();
}

FileHandle Multiprocess::WritePipeHandle() const {
  // This is the parent case, it's stdout in the child.
  return info_->pipe_p2c_write.get();
}

void Multiprocess::CloseReadPipe() {
  info_->pipe_c2p_read.reset();
}

void Multiprocess::CloseWritePipe() {
  info_->pipe_p2c_write.reset();
}

void Multiprocess::RunParent() {
  MultiprocessParent();

  info_->pipe_c2p_read.reset();
  info_->pipe_p2c_write.reset();
}

void Multiprocess::RunChild() {
  MultiprocessChild();

  info_->pipe_c2p_write.reset();
  info_->pipe_p2c_read.reset();
}

MultiprocessExec::MultiprocessExec()
    : Multiprocess(), command_(), arguments_(), command_line_() {
}

void MultiprocessExec::SetChildCommand(
    const std::string& command,
    const std::vector<std::string>* arguments) {
  command_ = command;
  if (arguments) {
    arguments_ = *arguments;
  } else {
    arguments_.clear();
  }
}

MultiprocessExec::~MultiprocessExec() {
}

void MultiprocessExec::PreFork() {
  ASSERT_FALSE(command_.empty());

  command_line_.clear();
  AppendCommandLineArgument(base::UTF8ToUTF16(command_), &command_line_);
  for (size_t i = 0; i < arguments_.size(); ++i) {
    command_line_ += L" ";
    AppendCommandLineArgument(base::UTF8ToUTF16(arguments_[i]), &command_line_);
  }

  // Make pipes for child-to-parent and parent-to-child communication. Mark them
  // as inheritable via the SECURITY_ATTRIBUTES, but use SetHandleInformation to
  // ensure that the parent sides are not inherited.
  ASSERT_EQ(nullptr, info());
  set_info(new internal::MultiprocessInfo());

  SECURITY_ATTRIBUTES security_attributes = {0};
  security_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
  security_attributes.bInheritHandle = TRUE;

  HANDLE c2p_read, c2p_write;
  PCHECK(CreatePipe(&c2p_read, &c2p_write, &security_attributes, 0));
  PCHECK(SetHandleInformation(c2p_read, HANDLE_FLAG_INHERIT, 0));
  info()->pipe_c2p_read.reset(c2p_read);
  info()->pipe_c2p_write.reset(c2p_write);

  HANDLE p2c_read, p2c_write;
  PCHECK(CreatePipe(&p2c_read, &p2c_write, &security_attributes, 0));
  PCHECK(SetHandleInformation(p2c_write, HANDLE_FLAG_INHERIT, 0));
  info()->pipe_p2c_read.reset(p2c_read);
  info()->pipe_p2c_write.reset(p2c_write);
}

void MultiprocessExec::MultiprocessChild() {
  STARTUPINFO startup_info = {0};
  startup_info.cb = sizeof(startup_info);
  startup_info.hStdInput = info()->pipe_p2c_read.get();
  startup_info.hStdOutput = info()->pipe_c2p_write.get();
  startup_info.hStdError = GetStdHandle(STD_ERROR_HANDLE);
  startup_info.dwFlags = STARTF_USESTDHANDLES;
  PCHECK(CreateProcess(base::UTF8ToUTF16(command_).c_str(),
                       &command_line_[0],  // This cannot be constant, per MSDN.
                       nullptr,
                       nullptr,
                       TRUE,
                       0,
                       nullptr,
                       nullptr,
                       &startup_info,
                       &info()->process_info));
}

}  // namespace test
}  // namespace crashpad
