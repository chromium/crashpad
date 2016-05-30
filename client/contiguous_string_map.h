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

#ifndef CRASHPAD_CLIENT_CONTIGUOUS_STRING_MAP_H_
#define CRASHPAD_CLIENT_CONTIGUOUS_STRING_MAP_H_

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <sstream>
#include <type_traits>

#include "base/strings/string_piece.h"

namespace crashpad {

//! \brief A map implementation using two blocks of contiguous storage.
//!
//! String to string map implemented via a string interning table and a sorted
//! set of pairs of indices into the string table. Both the string table and
//! the indices are maintained as two blocks of memory to allow the whole map
//! to be straightforward to IPC without a lot of pointer chasing.
class ContiguousStringMap {
 public:
  ContiguousStringMap() : entries_(128), string_table_(16384) {}

  //! \brief Copies and garbage collects string_table_.
  ContiguousStringMap(const ContiguousStringMap& rhs)
      : entries_(rhs.entries_.size()), string_table_(rhs.string_table_.size()) {
    for (uint32_t i = 0; i < rhs.entries_.size(); ++i) {
      Set(&rhs.string_table_[rhs.entries_[i].key],
          &rhs.string_table_[rhs.entries_[i].value]);
    }
  }
  void operator=(const ContiguousStringMap& rhs) = delete;

  //! \brief Set \a key = \a value. Any existing value will be overwritten.
  //!
  //! Neither \a key nor \a value may contain \0.
  void Set(const std::string& key, const std::string& value);

  //! \brief If \a key exists in the map, remove it an its associated value.
  //!
  //! \a key may not contain \0.
  void Remove(const std::string& key);

  //! \brief Returns the number of keys in the map.
  uint32_t Size() const { return entries_.size(); }

  //! \brief Returns the key/value pair at the given index.
  //!
  //! \param[in] i The iteration index to retrieve. Must be less than Size().
  std::pair<base::StringPiece, base::StringPiece> At(uint32_t i) const {
    return std::make_pair(&string_table_[entries_[i].key],
                          &string_table_[entries_[i].value]);
  }

  //! \brief Returns the string table indices for the key/value pair at the
  //!     given index.
  //!
  //! \note This method is primarily for testing purposes and is not generally
  //!     useful.
  //!
  //! \param[in] i The iteration index to retrieve. Must be less than Size().
  std::pair<uint32_t, uint32_t> StringTableIndicesAt(uint32_t i) const {
    return std::make_pair(entries_[i].key, entries_[i].value);
  }

 private:
  using InternedString = uint32_t;

  struct MapEntry {
    InternedString key;
    InternedString value;
  };

  InternedString Intern(const std::string& str);

  struct MapEntryPredicate {
    bool operator()(const MapEntry& a, const MapEntry& b) {
      return a.key < b.key;
    }
  };

  // IPC-suitable vector. A pointer and uint32_ts for size and capacity of the
  // data pointed to by the pointer. Note that the size and capacity are stored
  // in *bytes* to allow for simple IPC copy. Capacity is of course not
  // meanigful once it's been IPCd. Sizes are explicitly uint32_t rather than
  // size_t given the expected number of elements, and the use of indices into
  // SimpleBuffers as key/values in ContiguousStringMap.
  template <class T>
  class SimpleBuffer {
   public:
    static_assert(std::is_trivially_copyable<T>::value,
                  "can only hold trivially_copyable types");

    explicit SimpleBuffer(uint32_t initial_capacity)
        : data_(new T[initial_capacity]),
          size_(0),
          capacity_(initial_capacity * sizeof(T)) {}
    ~SimpleBuffer() { delete[] data_; }

    void push_back(const T& value) {
      if (size_ == capacity_)
        Grow();
      data_[size_ / sizeof(T)] = value;
      size_ += sizeof(T);
    }

    void erase(T* to_erase) {
      memmove(to_erase, to_erase + 1, (end() - to_erase - 1) * sizeof(T));
      size_ -= sizeof(T);
    }

    void reserve(uint32_t capacity) {
      while (capacity > capacity_)
        Grow();
    }

    uint32_t size() const { return size_ / sizeof(T); }
    uint32_t capacity() const { return capacity_ / sizeof(T); }
    T* begin() { return data_; }
    T* end() { return data_ + size_ / sizeof(T); }
    const T& operator[](uint32_t i) const { return data_[i]; }

   private:
    T* data_;
    uint32_t size_;
    uint32_t capacity_;

    void Grow() {
      capacity_ = capacity_ ? capacity_ * 2 : 10000;
      T* new_data = new T[capacity_];
      memcpy(new_data, data_, size_);
      delete[] data_;
      data_ = new_data;
    }

    SimpleBuffer(const SimpleBuffer&) = delete;
    void operator=(const SimpleBuffer&) = delete;
  };

  SimpleBuffer<MapEntry> entries_;
  SimpleBuffer<char> string_table_;
};

static_assert(sizeof(ContiguousStringMap) ==
                  (sizeof(void*) + sizeof(uint32_t) + sizeof(uint32_t)) * 2,
              "unexpected ContiguousStringMap size");

}  // namespace crashpad

#endif  // CRASHPAD_CLIENT_CONTIGUOUS_STRING_MAP_H_
