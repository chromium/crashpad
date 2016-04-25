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

#ifndef CRASHPAD_MINIDUMP_MINIDUMP_MEMORY_WRITER_H_
#define CRASHPAD_MINIDUMP_MINIDUMP_MEMORY_WRITER_H_

#include <windows.h>
#include <dbghelp.h>
#include <stdint.h>
#include <sys/types.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "minidump/minidump_stream_writer.h"
#include "minidump/minidump_writable.h"
#include "util/file/file_io.h"
#include "util/stdlib/pointer_container.h"

namespace crashpad {

class MemorySnapshot;

//! \brief The base class for writers of memory ranges pointed to by
//!     MINIDUMP_MEMORY_DESCRIPTOR objects in a minidump file.
//!
//! This is an abstract base class because users are expected to provide their
//! own implementations that, when possible, obtain the memory contents
//! on-demand in their WriteObject() methods. Memory ranges may be large, and
//! the alternative construction would require the contents of multiple ranges
//! to be held in memory simultaneously while a minidump file is being written.
class MinidumpMemoryWriter : public internal::MinidumpWritable {
 public:
  ~MinidumpMemoryWriter() override;

  //! \brief Creates a concrete initialized MinidumpMemoryWriter based on \a
  //!     memory_snapshot.
  //!
  //! \param[in] memory_snapshot The memory snapshot to use as source data.
  //!
  //! \return An object of a MinidumpMemoryWriter subclass initialized using the
  //!     source data in \a memory_snapshot.
  static std::unique_ptr<MinidumpMemoryWriter> CreateFromSnapshot(
      const MemorySnapshot* memory_snapshot);

  //! \brief Returns a MINIDUMP_MEMORY_DESCRIPTOR referencing the data that this
  //!     object writes.
  //!
  //! This method is expected to be called by a MinidumpMemoryListWriter in
  //! order to obtain a MINIDUMP_MEMORY_DESCRIPTOR to include in its list.
  //!
  //! \note Valid in #kStateWritable.
  const MINIDUMP_MEMORY_DESCRIPTOR* MinidumpMemoryDescriptor() const;

  //! \brief Registers a memory descriptor as one that should point to the
  //!     object on which this method is called.
  //!
  //! This method is expected to be called by objects of other classes, when
  //! those other classes have their own memory descriptors that need to point
  //! to memory ranges within a minidump file. MinidumpThreadWriter is one such
  //! class. This method is public for this reason, otherwise it would suffice
  //! to be private.
  //!
  //! \note Valid in #kStateFrozen or any preceding state.
  void RegisterMemoryDescriptor(MINIDUMP_MEMORY_DESCRIPTOR* memory_descriptor);

 protected:
  MinidumpMemoryWriter();

  //! \brief Returns the base address of the memory region in the address space
  //!     of the process that the snapshot describes.
  //!
  //! \note This method will only be called in #kStateFrozen.
  virtual uint64_t MemoryRangeBaseAddress() const = 0;

  //! \brief Returns the size of the memory region in bytes.
  //!
  //! \note This method will only be called in #kStateFrozen or a subsequent
  //!     state.
  virtual size_t MemoryRangeSize() const = 0;

  // MinidumpWritable:
  bool Freeze() override;
  size_t SizeOfObject() final;

  //! \brief Returns the object’s desired byte-boundary alignment.
  //!
  //! Memory regions are aligned to a 16-byte boundary. The actual alignment
  //! requirements of any data within the memory region are unknown, and may be
  //! more or less strict than this depending on the platform.
  //!
  //! \return `16`.
  //!
  //! \note Valid in #kStateFrozen or any subsequent state.
  size_t Alignment() override;

  bool WillWriteAtOffsetImpl(FileOffset offset) override;

  //! \brief Returns the object’s desired write phase.
  //!
  //! Memory regions are written at the end of minidump files, because it is
  //! expected that unlike most other data in a minidump file, the contents of
  //! memory regions will be accessed sparsely.
  //!
  //! \return #kPhaseLate.
  //!
  //! \note Valid in any state.
  Phase WritePhase() final;

 private:
  MINIDUMP_MEMORY_DESCRIPTOR memory_descriptor_;

  // weak
  std::vector<MINIDUMP_MEMORY_DESCRIPTOR*> registered_memory_descriptors_;

  DISALLOW_COPY_AND_ASSIGN(MinidumpMemoryWriter);
};

//! \brief The writer for a MINIDUMP_MEMORY_LIST stream in a minidump file,
//!     containing a list of MINIDUMP_MEMORY_DESCRIPTOR objects.
class MinidumpMemoryListWriter final : public internal::MinidumpStreamWriter {
 public:
  MinidumpMemoryListWriter();
  ~MinidumpMemoryListWriter() override;

  //! \brief Adds a concrete initialized MinidumpMemoryWriter for each memory
  //!     snapshot in \a memory_snapshots to the MINIDUMP_MEMORY_LIST.
  //!
  //! Memory snapshots are added in the fashion of AddMemory().
  //!
  //! \param[in] memory_snapshots The memory snapshots to use as source data.
  //!
  //! \note Valid in #kStateMutable.
  void AddFromSnapshot(
      const std::vector<const MemorySnapshot*>& memory_snapshots);

  //! \brief Adds a MinidumpMemoryWriter to the MINIDUMP_MEMORY_LIST.
  //!
  //! This object takes ownership of \a memory_writer and becomes its parent in
  //! the overall tree of internal::MinidumpWritable objects.
  //!
  //! \note Valid in #kStateMutable.
  void AddMemory(std::unique_ptr<MinidumpMemoryWriter> memory_writer);

  //! \brief Adds a MinidumpMemoryWriter that’s a child of another
  //!     internal::MinidumpWritable object to the MINIDUMP_MEMORY_LIST.
  //!
  //! \a memory_writer does not become a child of this object, but the
  //! MINIDUMP_MEMORY_LIST will still contain a MINIDUMP_MEMORY_DESCRIPTOR for
  //! it. \a memory_writer must be a child of another object in the
  //! internal::MinidumpWritable tree.
  //!
  //! This method exists to be called by objects that have their own
  //! MinidumpMemoryWriter children but wish for them to also appear in the
  //! minidump file’s MINIDUMP_MEMORY_LIST. MinidumpThreadWriter, which has a
  //! MinidumpMemoryWriter for thread stack memory, is an example.
  //!
  //! \note Valid in #kStateMutable.
  void AddExtraMemory(MinidumpMemoryWriter* memory_writer);

 protected:
  // MinidumpWritable:
  bool Freeze() override;
  size_t SizeOfObject() override;
  std::vector<MinidumpWritable*> Children() override;
  bool WriteObject(FileWriterInterface* file_writer) override;

  // MinidumpStreamWriter:
  MinidumpStreamType StreamType() const override;

 private:
  std::vector<MinidumpMemoryWriter*> memory_writers_;  // weak
  PointerVector<MinidumpMemoryWriter> children_;
  MINIDUMP_MEMORY_LIST memory_list_base_;

  DISALLOW_COPY_AND_ASSIGN(MinidumpMemoryListWriter);
};

}  // namespace crashpad

#endif  // CRASHPAD_MINIDUMP_MINIDUMP_MEMORY_WRITER_H_
