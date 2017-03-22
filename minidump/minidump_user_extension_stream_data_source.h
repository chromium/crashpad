// Copyright 2017 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_MINIDUMP_MINIDUMP_USER_EXTENSION_STREAM_DATA_SOURCE_H_
#define CRASHPAD_MINIDUMP_MINIDUMP_USER_EXTENSION_STREAM_DATA_SOURCE_H_

#include <stdint.h>
#include <sys/types.h>

#include "base/macros.h"

#include "minidump/minidump_extensions.h"

namespace crashpad {

//! \brief Describes a user extension data stream in a minidump.
class MinidumpUserExtensionStreamDataSource {
 public:
  //! \brief Constructs a MinidumpUserExtensionStreamDataSource.
  //!
  //! \param[in] stream_type The type of the user extension stream.
  //! \param[in] buffer Points to the data for this stream. \a buffer is not
  //!     owned, and must outlive the use of this object.
  //! \param[in] buffer_size The length of data in \a buffer.
  MinidumpUserExtensionStreamDataSource(uint32_t stream_type,
                                        const void* buffer,
                                        size_t buffer_size);
  ~MinidumpUserExtensionStreamDataSource();

  MinidumpStreamType stream_type() const { return stream_type_; }
  const void* buffer() const { return buffer_; }
  size_t buffer_size() const { return buffer_size_; }

 private:
  MinidumpStreamType stream_type_;
  const void* buffer_;  // weak
  size_t buffer_size_;

  DISALLOW_COPY_AND_ASSIGN(MinidumpUserExtensionStreamDataSource);
};

}  // namespace crashpad

#endif  // CRASHPAD_MINIDUMP_MINIDUMP_USER_EXTENSION_STREAM_DATA_SOURCE_H_
