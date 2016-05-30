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

#include "client/contiguous_string_map.h"

#include "base/logging.h"

namespace crashpad {

void ContiguousStringMap::Set(const std::string& key,
                              const std::string& value) {
  InternedString interned_key = Intern(key);
  InternedString interned_value = Intern(value);
  MapEntry to_insert = {interned_key, interned_value};
  auto it = std::lower_bound(
      entries_.begin(), entries_.end(), to_insert, MapEntryPredicate());
  if (it != entries_.end() && it->key == interned_key) {
    // Replace if the key already exists.
    it->value = interned_value;
  } else {
    // Otherwise, append and merge to correct location.
    entries_.push_back(to_insert);
    std::inplace_merge(entries_.begin(),
                       entries_.end() - 1,
                       entries_.end(),
                       MapEntryPredicate());
  }
}

void ContiguousStringMap::Remove(const std::string& key) {
  InternedString interned_key = Intern(key);
  MapEntry to_remove = {interned_key, 0};
  auto it = std::lower_bound(
      entries_.begin(), entries_.end(), to_remove, MapEntryPredicate());
  if (it != entries_.end() && it->key == interned_key)
    entries_.erase(it);
}

ContiguousStringMap::InternedString ContiguousStringMap::Intern(
    const std::string& str) {
  DCHECK_EQ(std::string::npos, str.find('\0'));

  // Find the string anywhere it exists in the string table already (including a
  // null terminator), and return its offset if we found it.
  const char* existing = reinterpret_cast<const char*>(memmem(
      string_table_.begin(), string_table_.size(), &str[0], str.size() + 1));
  if (existing)
    return static_cast<InternedString>(existing - string_table_.begin());

  // Otherwise, append the new string and return that.
  InternedString to_return = string_table_.size();
  string_table_.reserve(
      static_cast<uint32_t>(string_table_.size() + str.size() + 1));
  for (char c : str)
    string_table_.push_back(c);
  string_table_.push_back(0);
  return to_return;
}

}  // namespace crashpad
