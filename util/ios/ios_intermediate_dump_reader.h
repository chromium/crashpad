// Copyright 2021 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_UTIL_IOS_IOS_INTERMEDIATEDUMP_READER_H_
#define CRASHPAD_UTIL_IOS_IOS_INTERMEDIATEDUMP_READER_H_

#include "base/files/file_path.h"
#include "util/file/file_reader.h"
#include "util/ios/ios_intermediate_dump_map.h"

namespace crashpad {
namespace internal {

//! \brief Open and parse iOS intermediate dumps.
class IOSIntermediateDumpReader {
 public:
  IOSIntermediateDumpReader() {}

  //! \brief Open \a path for reading, ignoring empty files.
  //!
  //! \param[in] path The intermediate dump to read.
  //!
  //! \return On success, returns `true`, otherwise returns `false`.
  bool Initialize(const base::FilePath& path);

  //! \brief Generate a IOSIntermediateDumpMap based on an intermediate dump.
  //!
  //! Parse the binary file, similar to a JSON file, using the same format used
  //! by IOSIntermediateDumpWriter.
  bool Parse();

  //! \brief Returns an IOSIntermediateDumpMap corresponding to the root of the
  //!     intermediate dump.
  const IOSIntermediateDumpMap* RootMap() { return &minidump_; }

 private:
  bool ParseInternal(FileReaderInterface* reader,
                     IOSIntermediateDumpMap& mainDocument);
  std::unique_ptr<crashpad::FileReader> reader_;
  IOSIntermediateDumpMap minidump_;

  DISALLOW_COPY_AND_ASSIGN(IOSIntermediateDumpReader);
};

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_UTIL_IOS_IOS_INTERMEDIATEDUMP_READER_H_
