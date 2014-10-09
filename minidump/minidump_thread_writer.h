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

#ifndef CRASHPAD_MINIDUMP_MINIDUMP_THREAD_WRITER_H_
#define CRASHPAD_MINIDUMP_MINIDUMP_THREAD_WRITER_H_

#include <dbghelp.h>
#include <stdint.h>
#include <sys/types.h>

#include <vector>

#include "base/basictypes.h"
#include "minidump/minidump_stream_writer.h"
#include "minidump/minidump_writable.h"
#include "util/file/file_writer.h"

namespace crashpad {

class MinidumpContextWriter;
class MinidumpMemoryListWriter;
class MinidumpMemoryWriter;

//! \brief The writer for a MINIDUMP_THREAD object in a minidump file.
//!
//! Because MINIDUMP_THREAD objects only appear as elements of
//! MINIDUMP_THREAD_LIST objects, this class does not write any data on its own.
//! It makes its MINIDUMP_THREAD data available to its MinidumpThreadListWriter
//! parent, which writes it as part of a MINIDUMP_THREAD_LIST.
class MinidumpThreadWriter final : public internal::MinidumpWritable {
 public:
  MinidumpThreadWriter();
  ~MinidumpThreadWriter() {}

  //! \brief Returns a MINIDUMP_THREAD referencing this object’s data.
  //!
  //! This method is expected to be called by a MinidumpThreadListWriter in
  //! order to obtain a MINIDUMP_THREAD to include in its list.
  //!
  //! \note Valid in #kStateWritable.
  const MINIDUMP_THREAD* MinidumpThread() const;

  //! \brief Returns a MinidumpMemoryWriter that will write the memory region
  //!     corresponding to this object’s stack.
  //!
  //! If the thread does not have a stack, or its stack could not be determined,
  //! this will return NULL.
  //!
  //! This method is provided so that MinidumpThreadListWriter can obtain thread
  //! stack memory regions for the purposes of adding them to a
  //! MinidumpMemoryListWriter (configured by calling
  //! MinidumpThreadListWriter::SetMemoryListWriter()) by calling
  //! MinidumpMemoryListWriter::AddExtraMemory().
  //!
  //! \note Valid in any state.
  MinidumpMemoryWriter* Stack() const { return stack_; }

  //! \brief Arranges for MINIDUMP_THREAD::Stack to point to the MINIDUMP_MEMORY
  //!     object to be written by \a stack.
  //!
  //! \a stack will become a child of this object in the overall tree of
  //! internal::MinidumpWritable objects.
  //!
  //! \note Valid in #kStateMutable.
  void SetStack(MinidumpMemoryWriter* stack);

  //! \brief Arranges for MINIDUMP_THREAD::ThreadContext to point to the CPU
  //!     context to be written by \a context.
  //!
  //! A context is required in all MINIDUMP_THREAD objects.
  //!
  //! \a context will become a child of this object in the overall tree of
  //! internal::MinidumpWritable objects.
  //!
  //! \note Valid in #kStateMutable.
  void SetContext(MinidumpContextWriter* context);

  //! \brief Sets MINIDUMP_THREAD::ThreadId.
  void SetThreadID(uint32_t thread_id) { thread_.ThreadId = thread_id; }

  //! \brief Sets MINIDUMP_THREAD::SuspendCount.
  void SetSuspendCount(uint32_t suspend_count) {
    thread_.SuspendCount = suspend_count;
  }

  //! \brief Sets MINIDUMP_THREAD::PriorityClass.
  void SetPriorityClass(uint32_t priority_class) {
    thread_.PriorityClass = priority_class;
  }

  //! \brief Sets MINIDUMP_THREAD::Priority.
  void SetPriority(uint32_t priority) { thread_.Priority = priority; }

  //! \brief Sets MINIDUMP_THREAD::Teb.
  void SetTEB(uint64_t teb) { thread_.Teb = teb; }

 protected:
  // MinidumpWritable:
  virtual bool Freeze() override;
  virtual size_t SizeOfObject() override;
  virtual std::vector<MinidumpWritable*> Children() override;
  virtual bool WriteObject(FileWriterInterface* file_writer) override;

 private:
  MINIDUMP_THREAD thread_;
  MinidumpMemoryWriter* stack_;  // weak
  MinidumpContextWriter* context_;  // weak

  DISALLOW_COPY_AND_ASSIGN(MinidumpThreadWriter);
};

//! \brief The writer for a MINIDUMP_THREAD_LIST stream in a minidump file,
//!     containing a list of MINIDUMP_THREAD objects.
class MinidumpThreadListWriter final : public internal::MinidumpStreamWriter {
 public:
  MinidumpThreadListWriter();
  ~MinidumpThreadListWriter();

  //! \brief Sets the MinidumpMemoryListWriter that each thread’s stack memory
  //!     region should be added to as extra memory.
  //!
  //! Each MINIDUMP_THREAD object can contain a reference to a
  //! MinidumpMemoryWriter object that contains a snapshot of its stack memory.
  //! In the overall tree of internal::MinidumpWritable objects, these
  //! MinidumpMemoryWriter objects are considered children of their
  //! MINIDUMP_THREAD, and are referenced by a MINIDUMP_MEMORY_DESCRIPTOR
  //! contained in the MINIDUMP_THREAD. It is also possible for the same memory
  //! regions to have MINIDUMP_MEMORY_DESCRIPTOR objects present in a
  //! MINIDUMP_MEMORY_LIST stream. This is accomplished by calling this method,
  //! which informs a MinidumpThreadListWriter that it should call
  //! MinidumpMemoryListWriter::AddExtraMemory() for each extant thread stack
  //! while the thread is being added in AddThread(). When this is done, the
  //! MinidumpMemoryListWriter will contain a MINIDUMP_MEMORY_DESCRIPTOR
  //! pointing to the thread’s stack memory in its MINIDUMP_MEMORY_LIST. Note
  //! that the actual contents of the memory is only written once, as a child of
  //! the MinidumpThreadWriter. The MINIDUMP_MEMORY_DESCRIPTOR objects in both
  //! the MINIDUMP_THREAD and MINIDUMP_MEMORY_LIST will point to the same copy
  //! of the memory’s contents.
  //!
  //! \note This method must be called before AddThread() is called. Threads
  //!     added by AddThread() prior to this method being called will not have
  //!     their stacks added to \a memory_list_writer as extra memory.
  //! \note Valid in #kStateMutable.
  void SetMemoryListWriter(MinidumpMemoryListWriter* memory_list_writer);

  //! \brief Adds a MinidumpThreadWriter to the MINIDUMP_THREAD_LIST.
  //!
  //! \a thread will become a child of this object in the overall tree of
  //! internal::MinidumpWritable objects.
  //!
  //! \note Valid in #kStateMutable.
  void AddThread(MinidumpThreadWriter* thread);

 protected:
  // MinidumpWritable:
  virtual bool Freeze() override;
  virtual size_t SizeOfObject() override;
  virtual std::vector<MinidumpWritable*> Children() override;
  virtual bool WriteObject(FileWriterInterface* file_writer) override;

  // MinidumpStreamWriter:
  virtual MinidumpStreamType StreamType() const override;

 private:
  MINIDUMP_THREAD_LIST thread_list_base_;
  std::vector<MinidumpThreadWriter*> threads_;  // weak
  MinidumpMemoryListWriter* memory_list_writer_;  // weak

  DISALLOW_COPY_AND_ASSIGN(MinidumpThreadListWriter);
};

}  // namespace crashpad

#endif  // CRASHPAD_MINIDUMP_MINIDUMP_THREAD_WRITER_H_
