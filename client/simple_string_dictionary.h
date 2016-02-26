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
#include <sys/types.h>

#include "base/logging.h"
#include "base/macros.h"
#include "util/misc/implicit_cast.h"

namespace crashpad {

//! \brief A map/dictionary collection implementation using a fixed amount of
//!     storage, so that it does not perform any dynamic allocations for its
//!     operations.
//!
//! The actual map storage (TSimpleStringDictionary::Entry) is guaranteed to be
//! POD, so that it can be transmitted over various IPC mechanisms.
//!
//! The template parameters control the amount of storage used for the key,
//! value, and map. The \a KeySize and \a ValueSize are measured in bytes, not
//! glyphs, and include space for a trailing `NUL` byte. This gives space for
//! `KeySize - 1` and `ValueSize - 1` characters in an entry. \a NumEntries is
//! the total number of entries that will fit in the map.
template <size_t KeySize = 256, size_t ValueSize = 256, size_t NumEntries = 64>
class TSimpleStringDictionary {
 public:
  //! \brief Constant and publicly accessible versions of the template
  //!     parameters.
  //! \{
  static const size_t key_size = KeySize;
  static const size_t value_size = ValueSize;
  static const size_t num_entries = NumEntries;
  //! \}

  //! \brief A single entry in the map.
  struct Entry {
    //! \brief The entry’s key.
    //!
    //! If this is a 0-length `NUL`-terminated string, the entry is inactive.
    char key[KeySize];

    //! \brief The entry’s value.
    char value[ValueSize];

    //! \brief Returns the validity of the entry.
    //!
    //! If #key is an empty string, the entry is considered inactive, and this
    //! method returns `false`. Otherwise, returns `true`.
    bool is_active() const {
      return key[0] != '\0';
    }
  };

  //! \brief An iterator to traverse all of the active entries in a
  //!     TSimpleStringDictionary.
  class Iterator {
   public:
    explicit Iterator(const TSimpleStringDictionary& map)
        : map_(map),
          current_(0) {
    }

    //! \brief Returns the next entry in the map, or `nullptr` if at the end of
    //!     the collection.
    const Entry* Next() {
      while (current_ < map_.num_entries) {
        const Entry* entry = &map_.entries_[current_++];
        if (entry->is_active()) {
          return entry;
        }
      }
      return nullptr;
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

  //! \brief Returns the number of active key/value pairs. The upper limit for
  //!     this is \a NumEntries.
  size_t GetCount() const {
    size_t count = 0;
    for (size_t i = 0; i < num_entries; ++i) {
      if (entries_[i].is_active()) {
        ++count;
      }
    }
    return count;
  }

  //! \brief Given \a key, returns its corresponding value.
  //!
  //! \param[in] key The key to look up. This must not be `nullptr`.
  //!
  //! \return The corresponding value for \a key, or if \a key is not found,
  //!     `nullptr`.
  const char* GetValueForKey(const char* key) const {
    DCHECK(key);
    if (!key) {
      return nullptr;
    }

    const Entry* entry = GetConstEntryForKey(key);
    if (!entry) {
      return nullptr;
    }

    return entry->value;
  }

  //! \brief Stores \a value into \a key, replacing the existing value if \a key
  //!     is already present.
  //!
  //! If \a key is not yet in the map and the map is already full (containing
  //! \a NumEntries active entries), this operation silently fails.
  //!
  //! \param[in] key The key to store. This must not be `nullptr`.
  //! \param[in] value The value to store. If `nullptr`, \a key is removed from
  //!     the map.
  void SetKeyValue(const char* key, const char* value) {
    if (!value) {
      RemoveKey(key);
      return;
    }

    DCHECK(key);
    if (!key) {
      return;
    }

    // |key| must not be an empty string.
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

    // If the map is out of space, |entry| will be nullptr.
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

  //! \brief Removes \a key from the map.
  //!
  //! If \a key is not found, this is a no-op.
  //!
  //! \param[in] key The key of the entry to remove. This must not be `nullptr`.
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

    DCHECK_EQ(GetEntryForKey(key), implicit_cast<Entry*>(nullptr));
  }

 private:
  const Entry* GetConstEntryForKey(const char* key) const {
    for (size_t i = 0; i < num_entries; ++i) {
      if (strncmp(key, entries_[i].key, key_size) == 0) {
        return &entries_[i];
      }
    }
    return nullptr;
  }

  Entry* GetEntryForKey(const char* key) {
    return const_cast<Entry*>(GetConstEntryForKey(key));
  }

  Entry entries_[NumEntries];
};

//! \brief A TSimpleStringDictionary with default template parameters.
//!
//! For historical reasons this specialized version is available with the same
//! size factors as a previous implementation.
using SimpleStringDictionary = TSimpleStringDictionary<256, 256, 64>;

}  // namespace crashpad

#endif  // CRASHPAD_CLIENT_SIMPLE_STRING_DICTIONARY_H_
