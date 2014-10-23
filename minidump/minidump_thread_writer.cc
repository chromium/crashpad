// Copyright 2014 The Crashpad Authors. All rights reserved.
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

#include "minidump/minidump_thread_writer.h"

#include <sys/types.h>

#include "base/logging.h"
#include "minidump/minidump_context_writer.h"
#include "minidump/minidump_memory_writer.h"
#include "util/file/file_writer.h"
#include "util/numeric/safe_assignment.h"

namespace crashpad {

MinidumpThreadWriter::MinidumpThreadWriter()
    : MinidumpWritable(), thread_(), stack_(nullptr), context_(nullptr) {
}

const MINIDUMP_THREAD* MinidumpThreadWriter::MinidumpThread() const {
  DCHECK_EQ(state(), kStateWritable);

  return &thread_;
}

void MinidumpThreadWriter::SetStack(MinidumpMemoryWriter* stack) {
  DCHECK_EQ(state(), kStateMutable);

  stack_ = stack;
}

void MinidumpThreadWriter::SetContext(MinidumpContextWriter* context) {
  DCHECK_EQ(state(), kStateMutable);

  context_ = context;
}

bool MinidumpThreadWriter::Freeze() {
  DCHECK_EQ(state(), kStateMutable);
  CHECK(context_);

  if (!MinidumpWritable::Freeze()) {
    return false;
  }

  if (stack_) {
    stack_->RegisterMemoryDescriptor(&thread_.Stack);
  }

  context_->RegisterLocationDescriptor(&thread_.ThreadContext);

  return true;
}

size_t MinidumpThreadWriter::SizeOfObject() {
  DCHECK_GE(state(), kStateFrozen);

  // This object doesn’t directly write anything itself. Its MINIDUMP_THREAD is
  // written by its parent as part of a MINIDUMP_THREAD_LIST, and its children
  // are responsible for writing themselves.
  return 0;
}

std::vector<internal::MinidumpWritable*> MinidumpThreadWriter::Children() {
  DCHECK_GE(state(), kStateFrozen);
  DCHECK(context_);

  std::vector<MinidumpWritable*> children;
  if (stack_) {
    children.push_back(stack_);
  }
  children.push_back(context_);

  return children;
}

bool MinidumpThreadWriter::WriteObject(FileWriterInterface* file_writer) {
  DCHECK_EQ(state(), kStateWritable);

  // This object doesn’t directly write anything itself. Its MINIDUMP_THREAD is
  // written by its parent as part of a MINIDUMP_THREAD_LIST, and its children
  // are responsible for writing themselves.
  return true;
}

MinidumpThreadListWriter::MinidumpThreadListWriter()
    : MinidumpStreamWriter(),
      thread_list_base_(),
      threads_(),
      memory_list_writer_(nullptr) {
}

MinidumpThreadListWriter::~MinidumpThreadListWriter() {
}

void MinidumpThreadListWriter::SetMemoryListWriter(
    MinidumpMemoryListWriter* memory_list_writer) {
  DCHECK_EQ(state(), kStateMutable);
  DCHECK(threads_.empty());

  memory_list_writer_ = memory_list_writer;
}

void MinidumpThreadListWriter::AddThread(MinidumpThreadWriter* thread) {
  DCHECK_EQ(state(), kStateMutable);

  threads_.push_back(thread);

  if (memory_list_writer_) {
    MinidumpMemoryWriter* stack = thread->Stack();
    if (stack) {
      memory_list_writer_->AddExtraMemory(stack);
    }
  }
}

bool MinidumpThreadListWriter::Freeze() {
  DCHECK_EQ(state(), kStateMutable);

  if (!MinidumpStreamWriter::Freeze()) {
    return false;
  }

  size_t thread_count = threads_.size();
  if (!AssignIfInRange(&thread_list_base_.NumberOfThreads, thread_count)) {
    LOG(ERROR) << "thread_count " << thread_count << " out of range";
    return false;
  }

  return true;
}

size_t MinidumpThreadListWriter::SizeOfObject() {
  DCHECK_GE(state(), kStateFrozen);

  return sizeof(thread_list_base_) + threads_.size() * sizeof(MINIDUMP_THREAD);
}

std::vector<internal::MinidumpWritable*> MinidumpThreadListWriter::Children() {
  DCHECK_GE(state(), kStateFrozen);

  std::vector<MinidumpWritable*> children;
  for (MinidumpThreadWriter* thread : threads_) {
    children.push_back(thread);
  }

  return children;
}

bool MinidumpThreadListWriter::WriteObject(FileWriterInterface* file_writer) {
  DCHECK_EQ(state(), kStateWritable);

  WritableIoVec iov;
  iov.iov_base = &thread_list_base_;
  iov.iov_len = sizeof(thread_list_base_);
  std::vector<WritableIoVec> iovecs(1, iov);

  for (const MinidumpThreadWriter* thread : threads_) {
    iov.iov_base = thread->MinidumpThread();
    iov.iov_len = sizeof(MINIDUMP_THREAD);
    iovecs.push_back(iov);
  }

  return file_writer->WriteIoVec(&iovecs);
}

MinidumpStreamType MinidumpThreadListWriter::StreamType() const {
  return kMinidumpStreamTypeThreadList;
}

}  // namespace crashpad
