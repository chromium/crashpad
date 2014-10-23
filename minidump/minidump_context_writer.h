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

#ifndef CRASHPAD_MINIDUMP_MINIDUMP_CONTEXT_WRITER_H_
#define CRASHPAD_MINIDUMP_MINIDUMP_CONTEXT_WRITER_H_

#include <sys/types.h>

#include "base/basictypes.h"
#include "minidump/minidump_context.h"
#include "minidump/minidump_writable.h"

namespace crashpad {

//! \brief The base class for writers of CPU context structures in minidump
//!     files.
class MinidumpContextWriter : public internal::MinidumpWritable {
 public:
  virtual ~MinidumpContextWriter();

 protected:
  MinidumpContextWriter() : MinidumpWritable() {}

  //! \brief Returns the size of the context structure that this object will
  //!     write.
  //!
  //! \note This method will only be called in #kStateFrozen or a subsequent
  //!     state.
  virtual size_t ContextSize() const = 0;

  // MinidumpWritable:
  size_t SizeOfObject() final;

 private:
  DISALLOW_COPY_AND_ASSIGN(MinidumpContextWriter);
};

//! \brief The writer for a MinidumpContextX86 structure in a minidump file.
class MinidumpContextX86Writer final : public MinidumpContextWriter {
 public:
  MinidumpContextX86Writer();
  ~MinidumpContextX86Writer() override;

  //! \brief Returns a pointer to the context structure that this object will
  //!     write.
  //!
  //! \attention This returns a non-`const` pointer to this object’s private
  //!     data so that a caller can populate the context structure directly.
  //!     This is done because providing setter interfaces to each field in the
  //!     context structure would be unwieldy and cumbersome. Care must be taken
  //!     to populate the context structure correctly. The context structure
  //!     must only be modified while this object is in the #kStateMutable
  //!     state.
  MinidumpContextX86* context() { return &context_; }

 protected:
  // MinidumpWritable:
  bool WriteObject(FileWriterInterface* file_writer) override;

  // MinidumpContextWriter:
  size_t ContextSize() const override;

 private:
  MinidumpContextX86 context_;

  DISALLOW_COPY_AND_ASSIGN(MinidumpContextX86Writer);
};

//! \brief The writer for a MinidumpContextAMD64 structure in a minidump file.
class MinidumpContextAMD64Writer final : public MinidumpContextWriter {
 public:
  MinidumpContextAMD64Writer();
  ~MinidumpContextAMD64Writer() override;

  //! \brief Returns a pointer to the context structure that this object will
  //!     write.
  //!
  //! \attention This returns a non-`const` pointer to this object’s private
  //!     data so that a caller can populate the context structure directly.
  //!     This is done because providing setter interfaces to each field in the
  //!     context structure would be unwieldy and cumbersome. Care must be taken
  //!     to populate the context structure correctly. The context structure
  //!     must only be modified while this object is in the #kStateMutable
  //!     state.
  MinidumpContextAMD64* context() { return &context_; }

 protected:
  // MinidumpWritable:
  size_t Alignment() override;
  bool WriteObject(FileWriterInterface* file_writer) override;

  // MinidumpContextWriter:
  size_t ContextSize() const override;

 private:
  MinidumpContextAMD64 context_;

  DISALLOW_COPY_AND_ASSIGN(MinidumpContextAMD64Writer);
};

}  // namespace crashpad

#endif  // CRASHPAD_MINIDUMP_MINIDUMP_CONTEXT_WRITER_H_
