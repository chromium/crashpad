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

#include "util/linux/registration_protocol.h"

#include <vector>

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "util/stdlib/string_number_conversion.h"
#include "util/string/split_string.h"

namespace crashpad {

CrashDumpRequest::CrashDumpRequest()
    : client_process_id(-1), exception_information_address(0) {}

bool CrashDumpRequest::IsValid() const {
  return client_process_id >= 0;
}

bool CrashDumpRequest::InitializeFromString(const std::string& string) {
  std::vector<std::string> parts(SplitString(string, ','));
  if (parts.size() != 2) {
    LOG(ERROR) << "excpected 2 comma separated arguments";
    return false;
  }

  return StringToNumber(parts[0], &client_process_id) &&
         StringToNumber(parts[1], &exception_information_address);
}

std::string CrashDumpRequest::ToString() const {
  return base::StringPrintf(
      "%d,0x%" PRIx64, client_process_id, exception_information_address);
}

}  // namespace crashpad
