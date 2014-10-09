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

#ifndef CRASHPAD_MINIDUMP_MINIDUMP_EXCEPTION_WRITER_H_
#define CRASHPAD_MINIDUMP_MINIDUMP_EXCEPTION_WRITER_H_

#include <dbghelp.h>
#include <stdint.h>
#include <sys/types.h>

#include <vector>

#include "base/basictypes.h"
#include "minidump/minidump_context_writer.h"
#include "minidump/minidump_stream_writer.h"
#include "util/file/file_writer.h"

namespace crashpad {

//! \brief The writer for a MINIDUMP_EXCEPTION_STREAM stream in a minidump file.
class MinidumpExceptionWriter final : public internal::MinidumpStreamWriter {
 public:
  MinidumpExceptionWriter();
  ~MinidumpExceptionWriter() {}

  //! \brief Arranges for MINIDUMP_EXCEPTION_STREAM::ThreadContext to point to
  //!     the CPU context to be written by \a context.
  //!
  //! A context is required in all MINIDUMP_EXCEPTION_STREAM objects.
  //!
  //! \a context will become a child of this object in the overall tree of
  //! internal::MinidumpWritable objects.
  //!
  //! \note Valid in #kStateMutable.
  void SetContext(MinidumpContextWriter* context);

  //! \brief Sets MINIDUMP_EXCEPTION_STREAM::ThreadId.
  void SetThreadID(uint32_t thread_id) { exception_.ThreadId = thread_id; }

  //! \brief Sets MINIDUMP_EXCEPTION::ExceptionCode.
  void SetExceptionCode(uint32_t exception_code) {
    exception_.ExceptionRecord.ExceptionCode = exception_code;
  }

  //! \brief Sets MINIDUMP_EXCEPTION::ExceptionFlags.
  void SetExceptionFlags(uint32_t exception_flags) {
    exception_.ExceptionRecord.ExceptionFlags = exception_flags;
  }

  //! \brief Sets MINIDUMP_EXCEPTION::ExceptionRecord.
  void SetExceptionRecord(uint64_t exception_record) {
    exception_.ExceptionRecord.ExceptionRecord = exception_record;
  }

  //! \brief Sets MINIDUMP_EXCEPTION::ExceptionAddress.
  void SetExceptionAddress(uint64_t exception_address) {
    exception_.ExceptionRecord.ExceptionAddress = exception_address;
  }

  //! \brief Sets MINIDUMP_EXCEPTION::ExceptionInformation and
  //!     MINIDUMP_EXCEPTION::NumberParameters.
  //!
  //! MINIDUMP_EXCEPTION::NumberParameters is set to the number of elements in
  //! \a exception_information. The elements of
  //! MINIDUMP_EXCEPTION::ExceptionInformation are set to the elements of \a
  //! exception_information. Unused elements in
  //! MINIDUMP_EXCEPTION::ExceptionInformation are set to `0`.
  //!
  //! \a exception_information must have no more than
  //! #EXCEPTION_MAXIMUM_PARAMETERS elements.
  //!
  //! \note Valid in #kStateMutable.
  void SetExceptionInformation(
      const std::vector<uint64_t>& exception_information);

 protected:
  // MinidumpWritable:
  virtual bool Freeze() override;
  virtual size_t SizeOfObject() override;
  virtual std::vector<MinidumpWritable*> Children() override;
  virtual bool WriteObject(FileWriterInterface* file_writer) override;

  // MinidumpStreamWriter:
  virtual MinidumpStreamType StreamType() const override;

 private:
  MINIDUMP_EXCEPTION_STREAM exception_;
  MinidumpContextWriter* context_;  // weak

  DISALLOW_COPY_AND_ASSIGN(MinidumpExceptionWriter);
};

}  // namespace crashpad

#endif  // CRASHPAD_MINIDUMP_MINIDUMP_EXCEPTION_WRITER_H_
