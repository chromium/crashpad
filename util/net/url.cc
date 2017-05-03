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

#include "util/net/url.h"

#include <cctype>

namespace crashpad {

std::string URLEncode(const std::string& url) {
  const char kDecToHex[16 + 1] = "0123456789ABCDEF";
  std::string encoded;
  encoded.reserve(url.length());

  for (size_t i = 0; i < url.length(); ++i) {
    unsigned char character = static_cast<unsigned char>(url[i]);
    if (isalnum(character) || character == '-' || character == '_' ||
        character == '.' || character == '~') {
      // Copy unreserved character
      encoded += character;
    }
    else
    {
      // Escape character
      encoded += '%';
      encoded += kDecToHex[character >> 4];
      encoded += kDecToHex[character & 0xF];
    }
  }

  return encoded;
}

}  // namespace crashpad
