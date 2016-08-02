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

#ifndef CRASHPAD_CLIENT_ANNOTATIONS_H_
#define CRASHPAD_CLIENT_ANNOTATIONS_H_

#include <unordered_map>
#include <string>

#include "base/atomicops.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"

namespace crashpad {

//! \brief Storage for annotations (also known as crash keys).
//!
//! Conceptually a map of string-to-string, supporting add, update, and clear.
//! Designed to support out-of-process reading by the crash handler, and
//! modifies its data atomically so that the handler will get a consistent view
//! when reading.
//!
//! \ref https://docs.google.com/a/chromium.org/document/d/1UML8QM6uB19T6CrsvsSPOMeYgCAqk0ivVx1kPsM00VM/edit?usp=sharing.
class Annotations {
 public:
  Annotations();

  //! \brief Stores \a value into \a key, replacing the existing value if \a key
  //!     is already present.
  //!
  //! \param[in] key The key to store. This must not be `nullptr`.
  //! \param[in] value The key to store. If `nullptr`, \a this is equivalent to
  //!     ClearKey().
  void SetKeyValue(const char* key, const char* value);

  //! \brief Clears the value for \a key from the map.
  //!
  //! If \a key is not found, this is a no-op.
  //!
  //! \param[in] key The key of the entry to clear. This must not be `nullptr`.
  void ClearKey(const char* key);

  //! \brief Given \a key, returns its corresponding value.
  //!
  //! \param[in] key The key to look up. This must not be `nullptr`.
  //! \param[out] value The value, if \a key is in the map. May be `nullptr` to
  //!     do an existence test without retrieving the value.
  //!
  //! \return `true` if \a key is contained in the map with \a value set,
  //!     otherwise `false`.
  bool GetValueForKey(const char* key, std::string* value) const;

  //! \brief Returns the number of keys with non-null values.
  size_t GetCount() const;

  //! \brief Returns the number of keys in the map, regardless of the their
  //!     associated values.
  //!
  //! In particular, keys that have had their values cleared will still be
  //!     counted by this function.
  size_t GetNumKeys() const;

 private:
  //! \brief A single entry in the map.
  struct AnnotationsEntry {
    //! \brief A pointer to the next entry, linking all entries starting from
    //!     Annotations::head.
    AnnotationsEntry* next;

    //! \brief The entry's key.
    //!
    //! The string is owned and `NUL`-terminated.
    char* key;

    //! \brief The entry's value.
    //!
    //! This is an AtomicWord for implementation purposes, but is actually a
    //! char*. The string is owned and `NUL`-terminated.
    base::subtle::AtomicWord value;  // char*
  };

  void AddEntry(const char* key, const char* value);
  void UpdateValueForKey(AnnotationsEntry* entry, const char* value);

  //! \brief The head of the linked list of AnnotationsEntrys that are read by
  //!     the handler process.
  base::subtle::AtomicWord head_;  // AnnotationsEntry*

  //! \brief A cache mapping key names to elements of the linked list pointed
  //!     to by head_.
  std::unordered_map<base::StringPiece,
                     AnnotationsEntry*,
                     base::StringPieceHash> key_to_entry_;

  DISALLOW_COPY_AND_ASSIGN(Annotations);
};

}  // namespace crashpad

#endif  // CRASHPAD_CLIENT_ANNOTATIONS_H_
