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

#include "client/annotations.h"

#include "base/logging.h"

namespace crashpad {

Annotations::Annotations() : head_(0), key_to_entry_() {}

void Annotations::SetKeyValue(const char* key, const char* value) {
  DCHECK(key);
  if (!value) {
    ClearKey(key);
    return;
  }

  auto it = key_to_entry_.find(key);
  if (it == key_to_entry_.end()) {
    AddEntry(key, value);
  } else {
    UpdateValueForKey(it->second, value);
  }
}

void Annotations::ClearKey(const char* key) {
  DCHECK(key);
  auto it = key_to_entry_.find(key);
  if (it != key_to_entry_.end()) {
    base::subtle::AtomicWord old_value =
        base::subtle::NoBarrier_AtomicExchange(&it->second->value, 0);
    free(reinterpret_cast<char*>(old_value));
  }
}

bool Annotations::GetValueForKey(const char* key, std::string* value) const {
  DCHECK(key);
  auto it = key_to_entry_.find(key);
  if (it == key_to_entry_.end() || it->second->value == 0) {
    if (value)
      *value = std::string();
    return false;
  }
  if (value)
    *value = std::string(reinterpret_cast<char*>(it->second->value));
  return true;
}

size_t Annotations::GetCount() const {
  size_t size = 0;
  for (const auto& kv : key_to_entry_) {
    if (kv.second->value != 0)
      ++size;
  }
  return size;
}

size_t Annotations::GetNumKeys() const {
  return key_to_entry_.size();
}

void Annotations::AddEntry(const char* key, const char* value) {
  AnnotationsEntry* new_entry = new AnnotationsEntry();
  new_entry->next = reinterpret_cast<AnnotationsEntry*>(head_);
  new_entry->key = strdup(key);
  new_entry->value = reinterpret_cast<base::subtle::AtomicWord>(strdup(value));
  DCHECK(key_to_entry_.find(new_entry->key) == key_to_entry_.end());
  key_to_entry_[new_entry->key] = new_entry;
  base::subtle::NoBarrier_AtomicExchange(
      &head_, reinterpret_cast<base::subtle::AtomicWord>(new_entry));
}

void Annotations::UpdateValueForKey(AnnotationsEntry* entry,
                                    const char* value) {
  char* value_copy = strdup(value);
  base::subtle::AtomicWord old_value = base::subtle::NoBarrier_AtomicExchange(
      &entry->value, reinterpret_cast<base::subtle::AtomicWord>(value_copy));
  free(reinterpret_cast<char*>(old_value));
}

}  // namespace crashpad
