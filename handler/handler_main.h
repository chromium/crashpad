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

#ifndef CRASHPAD_HANDLER_HANDLER_MAIN_H_
#define CRASHPAD_HANDLER_HANDLER_MAIN_H_

#include <stdint.h>
#include <sys/types.h>

#include <map>
#include <memory>
#include <vector>

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace crashpad {

class ProcessSnapshot;

class UserStreamDataSource {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    //! \brief Called by  UserStreamDataSource::Read() to provide data requested
    //!     by a call to that method.
    //!
    //! \param[in] data A pointer to the data that was read. The callee does not
    //!     take ownership of this data. This data is only valid for the
    //!     duration of the call to this method. This parameter may be `nullptr`
    //!     if \a size is `0`.
    //! \param[in] size The size of the data that was read.
    //!
    //! \return `true` on success, `false` on failure.
    //!     UserStreamDataSource::ReadStreamData() will use this as its own
    //!     return value.
    virtual bool UserStreamDataSourceRead(const void* data, size_t size) = 0;
  };

  virtual ~UserStreamDataSource() {}

  // TODO(siggi): document me.
  virtual bool Initialize(ProcessSnapshot* process_snapshot) = 0;

  // TODO(siggi): document me.
  virtual size_t StreamDataSize() = 0;

  //! \brief Calls Delegate::UserStreamDataSourceRead(), providing it with
  //!     the stream data.
  //!
  //! Implementations do not necessarily comute the stream data prior to
  //! this method being called. The stream data may be computed or loaded lazily
  //! and may be discarded after being passed to the delegate.
  //!
  //! \return `false` on failure, otherwise, the return value of
  //!     Delegate::UserStreamDataSourceRead(), which should be `true` on
  //!     success and `false` on failure.
  virtual bool ReadStreamData(Delegate* delegate) = 0;
};

using UserStreamSources =
    std::map<uint32_t, std::unique_ptr<UserStreamDataSource>>;

//! \brief The `main()` of the `crashpad_handler` binary.
//!
//! This is exposed so that `crashpad_handler` can be embedded into another
//! binary, but called and used as if it were a standalone executable.
int HandlerMain(int argc, char* argv[]);

//! \brief The `main()` of the `crashpad_handler` binary with extensibility.
//!
//! This is allows running the Crashpad handler embedded into another binary,
//! with user extensibility stream sources.
//!
//! \param[in] user_stream_sources A map containing the extensibility data
//!     sources to call on crash. The resulting data streams will be added to
//!     the captured minidump.
int HandlerMainWithExtensibility(int argc,
                                 char* argv[],
                                 const UserStreamSources* user_stream_sources);

}  // namespace crashpad

#endif  // CRASHPAD_HANDLER_HANDLER_MAIN_H_
