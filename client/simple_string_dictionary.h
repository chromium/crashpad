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

#ifndef CRASHPAD_CLIENT_SIMPLE_STRING_DICTIONARY_H_
#define CRASHPAD_CLIENT_SIMPLE_STRING_DICTIONARY_H_

#include <string.h>

#include "base/basictypes.h"
#include "base/logging.h"

namespace crashpad {

// Opaque type for the serialized representation of a TSimpleStringDictionary.
// One is created in TSimpleStringDictionary::Serialize and can be deserialized
// using one of the constructors.
struct SerializedSimpleStringDictionary;

// TSimpleStringDictionary is an implementation of a map/dictionary collection
// that uses a fixed amount of storage, so that it does not perform any dynamic
// allocations for its operations.
//
// The actual map storage (the Entry) is guaranteed to be POD, so that it can be
// transmitted over various IPC mechanisms.
//
// The template parameters control the amount of storage used for the key,
// value, and map. The KeySize and ValueSize are measured in bytes, not glyphs,
// and includes space for a \0 byte. This gives space for KeySize-1 and
// ValueSize-1 characters in an entry. NumEntries is the total number of entries
// that will fit in the map.
template <size_t KeySize = 256, size_t ValueSize = 256, size_t NumEntries = 64>
class TSimpleStringDictionary {
 public:
  // Constant and publicly accessible versions of the template parameters.
  static const size_t key_size = KeySize;
  static const size_t value_size = ValueSize;
  static const size_t num_entries = NumEntries;

  // An Entry object is a single entry in the map. If the key is a 0-length
  // NUL-terminated string, the entry is empty.
  struct Entry {
    char key[KeySize];
    char value[ValueSize];

    bool is_active() const {
      return key[0] != '\0';
      }
  };

  // An Iterator can be used to iterate over all the active entries in a
  // TSimpleStringDictionary.
  class Iterator {
   public:
    explicit Iterator(const TSimpleStringDictionary& map)
        : map_(map),
          current_(0) {
    }

    // Returns the next entry in the map, or NULL if at the end of the
    // collection.
    const Entry* Next() {
      while (current_ < map_.num_entries) {
        const Entry* entry = &map_.entries_[current_++];
        if (entry->is_active()) {
          return entry;
        }
      }
      return NULL;
    }

   private:
    const TSimpleStringDictionary& map_;
    size_t current_;

    DISALLOW_COPY_AND_ASSIGN(Iterator);
  };

  TSimpleStringDictionary()
      : entries_() {
  }

  TSimpleStringDictionary(const TSimpleStringDictionary& other) {
    *this = other;
  }

  TSimpleStringDictionary& operator=(const TSimpleStringDictionary& other) {
    memcpy(entries_, other.entries_, sizeof(entries_));
    return *this;
  }

  // Constructs a map from its serialized form. |map| should be the out
  // parameter from Serialize() and |size| should be its return value.
  TSimpleStringDictionary(
      const SerializedSimpleStringDictionary* map, size_t size) {
    DCHECK_EQ(size, sizeof(entries_));
    if (size == sizeof(entries_)) {
      memcpy(entries_, map, size);
    }
  }

  // Returns the number of active key/value pairs. The upper limit for this is
  // NumEntries.
  size_t GetCount() const {
    size_t count = 0;
    for (size_t i = 0; i < num_entries; ++i) {
      if (entries_[i].is_active()) {
        ++count;
      }
    }
    return count;
  }

  // Given |key|, returns its corresponding |value|. |key| must not be NULL. If
  // the key is not found, NULL is returned.
  const char* GetValueForKey(const char* key) const {
    DCHECK(key);
    if (!key) {
      return NULL;
    }

    const Entry* entry = GetConstEntryForKey(key);
    if (!entry) {
      return NULL;
    }

    return entry->value;
  }

  // Stores |value| into |key|, replacing the existing value if |key| is already
  // present. |key| must not be NULL. If |value| is NULL, the key is removed
  // from the map. If there is no more space in the map, then the operation
  // silently fails.
  void SetKeyValue(const char* key, const char* value) {
    if (!value) {
      RemoveKey(key);
      return;
    }

    DCHECK(key);
    if (!key) {
      return;
    }

    // Key must not be an empty string.
    DCHECK_NE(key[0], '\0');
    if (key[0] == '\0') {
      return;
    }

    Entry* entry = GetEntryForKey(key);

    // If it does not yet exist, attempt to insert it.
    if (!entry) {
      for (size_t i = 0; i < num_entries; ++i) {
        if (!entries_[i].is_active()) {
          entry = &entries_[i];

          strncpy(entry->key, key, key_size);
          entry->key[key_size - 1] = '\0';

          break;
        }
      }
    }

    // If the map is out of space, entry will be NULL.
    if (!entry) {
      return;
    }

#ifndef NDEBUG
    // Sanity check that the key only appears once.
    int count = 0;
    for (size_t i = 0; i < num_entries; ++i) {
      if (strncmp(entries_[i].key, key, key_size) == 0) {
        ++count;
      }
    }
    DCHECK_EQ(count, 1);
#endif

    strncpy(entry->value, value, value_size);
    entry->value[value_size - 1] = '\0';
  }

  // Given |key|, removes any associated value. |key| must not be NULL. If the
  // key is not found, this is a noop.
  void RemoveKey(const char* key) {
    DCHECK(key);
    if (!key) {
      return;
    }

    Entry* entry = GetEntryForKey(key);
    if (entry) {
      entry->key[0] = '\0';
      entry->value[0] = '\0';
    }

    DCHECK_EQ(GetEntryForKey(key), static_cast<void*>(NULL));
  }

  // Places a serialized version of the map into |map| and returns the size.
  // Both of these should be passed to the deserializing constructor. Note that
  // the serialized |map| is scoped to the lifetime of the non-serialized
  // instance of this class. The |map| can be copied across IPC boundaries.
  size_t Serialize(const SerializedSimpleStringDictionary** map) const {
    *map = reinterpret_cast<const SerializedSimpleStringDictionary*>(entries_);
    return sizeof(entries_);
  }

 private:
  const Entry* GetConstEntryForKey(const char* key) const {
    for (size_t i = 0; i < num_entries; ++i) {
      if (strncmp(key, entries_[i].key, key_size) == 0) {
        return &entries_[i];
      }
    }
    return NULL;
  }

  Entry* GetEntryForKey(const char* key) {
    return const_cast<Entry*>(GetConstEntryForKey(key));
  }

  Entry entries_[NumEntries];
};

// For historical reasons this specialized version is available with the same
// size factors as a previous implementation.
typedef TSimpleStringDictionary<256, 256, 64> SimpleStringDictionary;

}  // namespace crashpad

#endif  // CRASHPAD_CLIENT_SIMPLE_STRING_DICTIONARY_H_
