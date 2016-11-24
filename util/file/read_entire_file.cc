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

#include "util/file/read_entire_file.h"

#include <type_traits>

#include "util/file/file_io.h"
#include "util/misc/implicit_cast.h"
#include "util/numeric/safe_assignment.h"
#include "util/stdlib/string_number_conversion.h"

namespace crashpad {

namespace {

template <typename ConvertType, typename ReturnType>
ReadFileResult ReadNumericOneLineFile(const base::FilePath& path,
                                      ReturnType* value) {
  std::string string;
  ReadFileResult result = ReadOneLineFile(path, &string);
  if (result != ReadFileResult::kSuccess) {
    return result;
  }

  ConvertType converted;
  if (!StringToNumber(string, &converted)) {
    LOG(ERROR) << "format error";
    return ReadFileResult::kFormatError;
  }

  if (!AssignIfInRange(value, converted)) {
    LOG(ERROR) << "value " << converted << " out of range";
    return ReadFileResult::kFormatError;
  }

  return ReadFileResult::kSuccess;
}

}  // namespace

ReadFileResult ReadEntireFile(const base::FilePath& path,
                              std::string* contents) {
  ScopedFileHandle handle(OpenFileForRead(path));
  if (handle == kInvalidFileHandle) {
    return ReadFileResult::kOpenError;
  }

  std::string local_contents;
  FileOperationResult rv;
  do {
    char buf[4096];
    rv = ReadFile(handle.get(), buf, sizeof(buf));
    if (rv < 0) {
      PLOG(ERROR) << "read";
      return ReadFileResult::kReadError;
    } else if (rv > implicit_cast<FileOperationResult>(sizeof(buf))) {
      LOG(ERROR) << "read: expected <= " << sizeof(buf) << ", observed " << rv;
      return ReadFileResult::kReadError;
    }
    local_contents.append(buf, rv);
  } while (rv != 0);

  contents->swap(local_contents);
  return ReadFileResult::kSuccess;
}

ReadFileResult ReadEntireFile(const base::FilePath& path,
                              std::vector<std::string>* lines) {
  std::string contents;
  ReadFileResult result = ReadEntireFile(path, &contents);
  if (result != ReadFileResult::kSuccess) {
    return result;
  }

  std::vector<std::string> local_lines;
  size_t start = 0;
  size_t end;
  while ((end = contents.find_first_of('\n', start)) != std::string::npos) {
    local_lines.push_back(contents.substr(start, end - start));
    start = end + 1;
  }
  if (start != contents.size()) {
    LOG(ERROR) << "unterminated line";
    return ReadFileResult::kFormatError;
  }

  lines->swap(local_lines);
  return ReadFileResult::kSuccess;
}

ReadFileResult ReadOneLineFile(const base::FilePath& path, std::string* value) {
  std::vector<std::string> lines;
  ReadFileResult result = ReadEntireFile(path, &lines);
  if (result != ReadFileResult::kSuccess) {
    return result;
  }

  if (lines.size() != 1) {
    LOG(ERROR) << "expected 1 line, observed " << lines.size();
    return ReadFileResult::kFormatError;
  }

  value->swap(lines[0]);
  return ReadFileResult::kSuccess;
}

ReadFileResult ReadOneLineFile(const base::FilePath& path, char* value) {
  if (std::is_signed<char>::value) {
    return ReadNumericOneLineFile<int>(path, value);
  } else {
    return ReadNumericOneLineFile<unsigned int>(path, value);
  }
}

ReadFileResult ReadOneLineFile(const base::FilePath& path, signed char* value) {
  return ReadNumericOneLineFile<int>(path, value);
}

ReadFileResult ReadOneLineFile(const base::FilePath& path,
                               unsigned char* value) {
  return ReadNumericOneLineFile<unsigned int>(path, value);
}

ReadFileResult ReadOneLineFile(const base::FilePath& path, short* value) {
  return ReadNumericOneLineFile<int>(path, value);
}

ReadFileResult ReadOneLineFile(const base::FilePath& path,
                               unsigned short* value) {
  return ReadNumericOneLineFile<unsigned int>(path, value);
}

ReadFileResult ReadOneLineFile(const base::FilePath& path, int* value) {
  return ReadNumericOneLineFile<int>(path, value);
}

ReadFileResult ReadOneLineFile(const base::FilePath& path,
                               unsigned int* value) {
  return ReadNumericOneLineFile<unsigned int>(path, value);
}

ReadFileResult ReadOneLineFile(const base::FilePath& path, long* value) {
  return ReadNumericOneLineFile<int64_t>(path, value);
}

ReadFileResult ReadOneLineFile(const base::FilePath& path,
                               unsigned long* value) {
  return ReadNumericOneLineFile<uint64_t>(path, value);
}

ReadFileResult ReadOneLineFile(const base::FilePath& path, long long* value) {
  return ReadNumericOneLineFile<int64_t>(path, value);
}

ReadFileResult ReadOneLineFile(const base::FilePath& path,
                               unsigned long long* value) {
  return ReadNumericOneLineFile<uint64_t>(path, value);
}

}  // namespace crashpad
