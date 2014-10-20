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

#ifndef CRASHPAD_MINIDUMP_TEST_MINIDUMP_STRING_WRITER_TEST_UTIL_H_
#define CRASHPAD_MINIDUMP_TEST_MINIDUMP_STRING_WRITER_TEST_UTIL_H_

#include <dbghelp.h>

#include <string>

#include "util/file/string_file_writer.h"

namespace crashpad {
namespace test {

//! \brief Returns the contents of a MinidumpUTF8String.
//!
//! If \a rva points outside of the range of \a file_writer, or if any of the
//! string data would lie outside of the range of \a file_writer, this function
//! will fail.
//!
//! \param[in] file_writer A StringFileWriter into which MinidumpWritable
//!     objects have been written.
//! \param[in] rva An offset in \a file_writer at which to find the desired
//!     string.
//!
//! \return On success, the string read from \a file_writer at offset \a rva. On
//!     failure, returns an empty string, with a nonfatal assertion logged to
//!     gtest.
std::string MinidumpUTF8StringAtRVA(const StringFileWriter& file_writer,
                                    RVA rva);

}  // namespace test
}  // namespace crashpad

#endif  // CRASHPAD_MINIDUMP_TEST_MINIDUMP_STRING_WRITER_TEST_UTIL_H_
