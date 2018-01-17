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

#include "test/multiprocess_exec.h"

#include <launchpad/launchpad.h>
#include <zircon/process.h>
#include <zircon/syscalls.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "gtest/gtest.h"

namespace crashpad {
namespace test {

namespace internal {

struct MultiprocessInfo {
  MultiprocessInfo() {}
  int stdin_write;
  int stdout_read;
  zx_handle_t child;
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
  zx_signals_t signals;
  ASSERT_EQ(zx_object_wait_one(
                info_->child, ZX_TASK_TERMINATED, ZX_TIME_INFINITE, &signals),
            ZX_OK);
  ASSERT_EQ(signals, ZX_TASK_TERMINATED);

  zx_handle_close(info_->child);
}

Multiprocess::~Multiprocess() {
  delete info_;
}

void Multiprocess::PreFork() {
  NOTREACHED();
}

FileHandle Multiprocess::ReadPipeHandle() const {
  return info_->stdout_read;
}

FileHandle Multiprocess::WritePipeHandle() const {
  return info_->stdin_write;
}

void Multiprocess::CloseReadPipe() {
  close(info_->stdout_read);
}

void Multiprocess::CloseWritePipe() {
  close(info_->stdin_write);
}

void Multiprocess::RunParent() {
  MultiprocessParent();

  close(info_->stdout_read);
  close(info_->stdin_write);
}

void Multiprocess::RunChild() {
  MultiprocessChild();
}

MultiprocessExec::MultiprocessExec()
    : Multiprocess(), command_(), arguments_(), argv_() {
}

void MultiprocessExec::SetChildCommand(
    const base::FilePath& command,
    const std::vector<std::string>* arguments) {
  command_ = command;
  if (arguments) {
    arguments_ = *arguments;
  } else {
    arguments_.clear();
  }
}

MultiprocessExec::~MultiprocessExec() {}

void MultiprocessExec::PreFork() {
  ASSERT_FALSE(command_.empty());

  ASSERT_TRUE(argv_.empty());

  argv_.push_back(command_.value().c_str());
  for (const std::string& argument : arguments_) {
    argv_.push_back(argument.c_str());
  }

  ASSERT_EQ(info(), nullptr);
  set_info(new internal::MultiprocessInfo());
}

void MultiprocessExec::MultiprocessChild() {
  launchpad_t* lp;
  launchpad_create(zx_job_default(), command_.value().c_str(), &lp);
  launchpad_load_from_file(lp, command_.value().c_str());
  launchpad_set_args(lp, argv_.size(), &argv_[0]);
  launchpad_add_pipe(lp, &info()->stdin_write, STDIN_FILENO);
  launchpad_add_pipe(lp, &info()->stdout_read, STDOUT_FILENO);

  const char* error_message;
  zx_status_t status = launchpad_go(lp, &info()->child, &error_message);
  ZX_CHECK(status == ZX_OK, status) << "launchpad_go: " << error_message;
}

}  // namespace test
}  // namespace crashpad
