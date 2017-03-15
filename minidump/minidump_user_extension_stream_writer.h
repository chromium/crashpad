// Copyright 2016 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_MINIDUMP_MINIDUMP_USER_EXTENSION_STREAM_WRITER_H_
#define CRASHPAD_MINIDUMP_MINIDUMP_USER_EXTENSION_STREAM_WRITER_H_

#include <windows.h>
#include <dbghelp.h>
#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "minidump/minidump_stream_writer.h"
#include "minidump/minidump_writable.h"
#include "snapshot/module_snapshot.h"

namespace crashpad {

class MinidumpUserExtensionStream;

//! \brief The writer for a MINIDUMP_USER_STREAM in a minidump file.
class MinidumpUserExtensionStreamWriter final
    : public internal::MinidumpStreamWriter {
 public:
  MinidumpUserExtensionStreamWriter();
  ~MinidumpUserExtensionStreamWriter() override;

  //! \brief Initializes a MINIDUMP_USER_STREAM based on \a extension_stream.
  //!
  //! \param[in] extension_stream The memory and stream type to use as source
  //! data.
  //!
  //! \note Valid in #kStateMutable.
  void InitializeFromSnapshot(
      const MinidumpUserExtensionStream* extension_stream);

 protected:
  // MinidumpWritable:
  bool Freeze() override;
  size_t SizeOfObject() override;
  std::vector<internal::MinidumpWritable*> Children() override;
  bool WriteObject(FileWriterInterface* file_writer) override;

  // MinidumpStreamWriter:
  MinidumpStreamType StreamType() const override;

 private:
  uint32_t stream_type_;
  // TODO(siggi): The data, man, the data. DO NOT SUBMIT.

  DISALLOW_COPY_AND_ASSIGN(MinidumpUserExtensionStreamWriter);
};

}  // namespace crashpad

#endif  // CRASHPAD_MINIDUMP_MINIDUMP_USER_EXTENSION_STREAM_WRITER_H_
