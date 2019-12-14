// Copyright 2019 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_UTIL_STREAM_LOG_FILE_PROCESSOR_H_
#define CRASHPAD_UTIL_STREAM_LOG_FILE_PROCESSOR_H_

#include "base/files/file_path.h"
#include "base/macros.h"

namespace crashpad {

//! \brief The class is used to write data to a file.
class LogFileProcessor {
 public:
   //! \brief Whether this object is configured to encode or decode data.
  enum class Mode : bool {
    //! \brief Data passed through this object is encoded.
    kEncode = false,
    //! \brief Data passed through this object is decoded.
    kDecode = true
  };

  LogFileProcessor(
    Mode mode,
    const base::FilePath& input,
    const base::FilePath& output);
  ~LogFileProcessor();

  bool Process();

 private:
  Mode mode_;
  base::FilePath input_;
  base::FilePath output_;

  DISALLOW_COPY_AND_ASSIGN(LogFileProcessor);
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_STREAM_LOG_FILE_PROCESSOR_H_
