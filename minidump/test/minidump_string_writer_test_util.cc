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

#include "minidump/test/minidump_string_writer_test_util.h"

#include "gtest/gtest.h"
#include "minidump/minidump_extensions.h"

namespace crashpad {
namespace test {

std::string MinidumpUTF8StringAtRVA(const StringFileWriter& file_writer,
                                    RVA rva) {
  const std::string& contents = file_writer.string();
  if (rva == 0) {
    return std::string();
  }

  if (rva + sizeof(MinidumpUTF8String) > contents.size()) {
    ADD_FAILURE()
        << "rva " << rva << " too large for contents " << contents.size();
    return std::string();
  }

  const MinidumpUTF8String* minidump_string =
      reinterpret_cast<const MinidumpUTF8String*>(&contents[rva]);

  // Verify that the file has enough data for the string’s stated length plus
  // its required NUL terminator.
  if (rva + sizeof(MinidumpUTF8String) + minidump_string->Length + 1 >
          contents.size()) {
    ADD_FAILURE()
        << "rva " << rva << ", length " << minidump_string->Length
        << " too large for contents " << contents.size();
    return std::string();
  }

  std::string minidump_string_data(
      reinterpret_cast<const char*>(&minidump_string->Buffer[0]),
      minidump_string->Length);
  return minidump_string_data;
}

}  // namespace test
}  // namespace crashpad
