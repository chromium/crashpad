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

#include "util/net/url.h"

#include <string.h>

#include "base/strings/stringprintf.h"

namespace crashpad {

std::string URLEncode(const std::string& url) {
  const char kSafeCharacters[] = "-_.~";
  std::string encoded;
  encoded.reserve(url.length());

  for (unsigned char character : url) {
    if (((character >= 'A') && (character <= 'Z')) ||
        ((character >= 'a') && (character <= 'z')) ||
        ((character >= '0') && (character <= '9')) ||
        (strchr(kSafeCharacters, character) != nullptr)) {
      // Copy unreserved character.
      encoded += character;
    } else {
      // Escape character.
      encoded += base::StringPrintf("%%%02X", character);
    }
  }

  return encoded;
}

}  // namespace crashpad
