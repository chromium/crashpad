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

#ifndef CRASHPAD_CLIENT_SIMPLE_ADDRESS_RANGE_BAG_H_
#define CRASHPAD_CLIENT_SIMPLE_ADDRESS_RANGE_BAG_H_

#include "base/logging.h"
#include "base/macros.h"
#include "util/numeric/checked_range.h"

namespace crashpad {

// Opaque type for the serialized representation of a TSimpleAddressRangeBag.
// One is created in TSimpleAddressRangeBag::Serialize and can be deserialized
// using one of the constructors.
struct SerializedSimpleAddressRangeBag;

//! \brief A bag implementation using a fixed amount of storage, so that it does
//!     not perform any dynamic allocations for its operations.
//!
//! The actual bag storage (TSimpleAddressRangeBag::Entry) is POD, so that it
//! can be transmitted over various IPC mechanisms.
template <size_t NumEntries = 64>
class TSimpleAddressRangeBag {
 public:
  //! Constant and publicly accessible version of the template parameter.
  static const size_t num_entries = NumEntries;

  //! \brief A single entry in the bag.
  struct Entry {
    //! \brief The base address of the range.
    uint64_t base;

    //! \brief The size of the range in bytes.
    uint64_t size;

    //! \brief Returns the validity of the entry.
    //!
    //! If #base and #size are both zero, the entry is considered inactive, and
    //! this method returns `false`. Otherwise, returns `true`.
    bool is_active() const {
      return base != 0 || size != 0;
    }
  };

  //! \brief An iterator to traverse all of the active entries in a
  //!     TSimpleAddressRangeBag.
  class Iterator {
   public:
    explicit Iterator(const TSimpleAddressRangeBag& bag)
        : bag_(bag),
          current_(0) {
    }

    //! \brief Returns the next entry in the bag, or `nullptr` if at the end of
    //!     the collection.
    const Entry* Next() {
      while (current_ < bag_.num_entries) {
        const Entry* entry = &bag_.entries_[current_++];
        if (entry->is_active()) {
          return entry;
        }
      }
      return nullptr;
    }

   private:
    const TSimpleAddressRangeBag& bag_;
    size_t current_;

    DISALLOW_COPY_AND_ASSIGN(Iterator);
  };

  TSimpleAddressRangeBag()
      : entries_() {
  }

  TSimpleAddressRangeBag(const TSimpleAddressRangeBag& other) {
    *this = other;
  }

  TSimpleAddressRangeBag& operator=(const TSimpleAddressRangeBag& other) {
    memcpy(entries_, other.entries_, sizeof(entries_));
    return *this;
  }

  //! \brief Constructs a bag from its serialized form. \a bag should be the out
  //!     parameter from Serialize(), and \a size should be its return value.
  TSimpleAddressRangeBag(const SerializedSimpleAddressRangeBag* bag,
                         size_t size) {
    DCHECK_EQ(size, sizeof(entries_));
    if (size == sizeof(entries_)) {
      memcpy(entries_, bag, size);
    }
  }

  //! \brief Returns the number of active entries. The upper limit for this is
  //!     \a NumEntries.
  size_t GetCount() const {
    size_t count = 0;
    for (size_t i = 0; i < num_entries; ++i) {
      if (entries_[i].is_active()) {
        ++count;
      }
    }
    return count;
  }

  //! \brief Inserts the given range into the bag.
  //!
  //! \param[in] range The range to be inserted. The range must have either a
  //!     non-zero base address or size.
  //!
  //! \return `true` if there was space to insert the range into the bag,
  //!     otherwise `false` with an error logged.
  bool Insert(CheckedRange<uint64_t> range) {
    DCHECK(range.base() != 0 || range.size() != 0);

    for (size_t i = 0; i < num_entries; ++i) {
      if (!entries_[i].is_active()) {
        entries_[i].base = range.base();
        entries_[i].size = range.size();
        return true;
      }
    }

    LOG(ERROR) << "no space available to insert range";
    return false;
  }

  //! \brief Inserts the given range into the bag.
  //!
  //! \param[in] base The base of the range to be inserted. May not be null.
  //! \param[in] size The size of the range to be inserted. May not be zero.
  //!
  //! \return `true` if there was space to insert the range into the bag,
  //!     otherwise `false` with an error logged.
  bool Insert(void* base, size_t size) {
    DCHECK(base != nullptr);
    DCHECK_NE(0u, size);
    return Insert(CheckedRange<uint64_t>(
        base::checked_cast<uint64_t>(reinterpret_cast<uintptr_t>(base)),
        base::checked_cast<uint64_t>(size)));
  }

  //! \brief Removes the given range from the bag.
  //!
  //! \param[in] range The range to be removed. The range must have either a
  //!     non-zero base address or size.
  //!
  //! \return `true` if the range was found and removed, otherwise `false` with
  //!     an error logged.
  bool Remove(CheckedRange<uint64_t> range) {
    DCHECK(range.base() != 0 || range.size() != 0);

    for (size_t i = 0; i < num_entries; ++i) {
      if (entries_[i].base == range.base() &&
          entries_[i].size == range.size()) {
        entries_[i].base = entries[i].size = 0;
        return true;
      }
    }

    LOG(ERROR) << "did not find range to remove";
    return false;
  }

  //! \brief Returns a serialized form of the bag.
  //!
  //! Places a serialized version of the bag into \a bag and returns the size in
  //! bytes. Both \a bag and the size should be passed to the deserializing
  //! constructor. Note that the serialized \a bag is scoped to the lifetime of
  //! the non-serialized instance of this class. The \a bag data can be copied
  //! across IPC boundaries.
  size_t Serialize(const SerializedSimpleAddressRangeBag** bag) const {
    *bag = reinterpret_cast<const SerializedSimpleAddressRangeBag*>(entries_);
    return sizeof(entries_);
  }

 private:
  Entry entries_[NumEntries];
};

//! \brief A TSimpleAddressRangeBag with default template parameters.
using SimpleAddressRangeBag = TSimpleAddressRangeBag<64>;

}  // namespace crashpad

#endif  // CRASHPAD_CLIENT_SIMPLE_ADDRESS_RANGE_BAG_H_
