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

#include <memory>
#include <vector>

namespace crashpad {

class ProcessSnapshot;
class MinidumpUserExtensionStreamDataSource;

//! \brief Extensibility interface for embedders who wish to add custom streams
//!     to minidumps.
class UserStreamDataSource {
 public:
  virtual ~UserStreamDataSource() {}

  //! \brief Produce the contents for an extension stream for a crashed program.
  //!
  //! Called after \a process_snapshot has been initialized for the crashed
  //! process to (optionally) produce the contents of a user extension stream
  //! that will be attached to the minidump.
  //!
  //! \param[in] process_snapshot An initialized snapshot for the crashed
  //!     process.
  //!
  //! \return A new data source for the stream to add to the minidump or
  //!      `nullptr` on failure or to opt out of adding a stream.
  virtual std::unique_ptr<MinidumpUserExtensionStreamDataSource>
  ProduceStreamData(ProcessSnapshot* process_snapshot) = 0;
};

using UserStreamDataSources =
    std::vector<std::unique_ptr<UserStreamDataSource>>;

//! \brief The `main()` of the `crashpad_handler` binary.
//!
//! This is exposed so that `crashpad_handler` can be embedded into another
//! binary, but called and used as if it were a standalone executable.
//!
//! \param[in] user_stream_sources A vector containing the extensibility data
//!     sources to call on crash. Each time a minidump is created, the sources
//!     are called in turn. Any streams returned are added to the dump.
int HandlerMain(int argc,
                char* argv[],
                const UserStreamDataSources* user_stream_sources);

}  // namespace crashpad

#endif  // CRASHPAD_HANDLER_HANDLER_MAIN_H_
