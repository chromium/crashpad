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

#ifndef CRASHPAD_CLIENT_ANNOTATION_H_
#define CRASHPAD_CLIENT_ANNOTATION_H_

#include <algorithm>
#include <atomic>

#include "base/macros.h"
#include "base/strings/string_util.h"

namespace crashpad {

class AnnotationList;

//! \brief Base class for an annotation, which records a key-value pair of
//!     arbitrary data when set.
//!
//! After an annotation is declared, its `value_ptr_` will not be captured in a
//! crash report until a call to \a SetSize() specifies how much data from the
//! value should be recorded.
//!
//! Annotations should be declared in the file-scope as static data in the file
//! in which they are used. An example declaration and usage:
//!
//! \code
//!   // foo.cc:
//!
//!   char g_buffer[PAGE_SIZE];
//!   crashpad::Annotation g_buffer_annotation{
//!       crashpad::Annotation::Type::kString, "buffer_head", g_buffer};
//!
//!   void OnBufferProduced(size_t n) {
//!     // Capture the head of the buffer, in case we crash when parsing it.
//!     g_buffer_annotation.SetSize(std::min(64, n));
//!     // Start parsing the header.
//!     Frobinate(g_buffer, n);
//!   }
//! \endcode
//!
//! Annotation objects are not threadsafe, and to manipulate them from other
//! threads, external synchronization must be used.
class Annotation {
 public:
  //! \brief The type of data stored in the annotation.
  enum class Type : uint16_t {
    //! \brief An invalid annotation. Reserved for internal use.
    kInvalid = 0,

    //! \brief A `NUL`-terminated C-string.
    kString = 1,

    //! \brief Clients may declare their own custom types by using values
    //!     greater than this.
    kUserDefinedStart = 0x7FFF,
  };

  //! \brief Constructs a new annotation.
  //!
  //! Upon construction, the annotation will not be included in any crash
  //! reports until \sa SetSize() is called with a value greater than `0`.
  //!
  //! \param[in] type The data type of the value of the annotation.
  //! \param[in] key A `NUL`-terminated C-string name for the annotation. Keys
  //!     do not have to be unique, though not all crash processors may handle
  //!     Annotations with the same key. Keys should be static, constexpr data.
  //! \param[in] value_ptr A pointer to the value for the key. The pointer may
  //!     not be changed once associated with an annotation, but the data may
  //!     be mutated.
  constexpr Annotation(Type type, const char key[], void* const value_ptr)
      : link_node_(nullptr),
        key_(key),
        value_ptr_(value_ptr),
        size_(0),
        type_(type) {}

  //! \brief Specifies the number of bytes in \a value_ptr_ to include when
  //!     generating a crash report.
  //!
  //! A size of `0` indicates that no value should be recorded and is the
  //! equivalent of calling \sa Clear().
  //!
  //! This method does not mutate the data referenced by the annotation, it
  //! merely updates the annotation system's bookkeeping.
  //!
  //! Subclasses of this base class that provide additional Set methods to
  //! mutate the value of the annotation must call always call this method.
  //!
  //! \param[in] size The number of bytes.
  void SetSize(uint32_t size);

  //! \brief Marks the annotation as cleared, indicating the \a value_ptr_
  //!     should not be included in a crash report.
  //!
  //! This method does not mutate the data referenced by the annotation, it
  //! merely updates the annotation system's bookkeeping.
  void Clear();

  //! \brief Tests whether the annotation has been set.
  bool is_set() const { return size_ > 0; }

  Type type() const { return type_; }
  uint32_t size() const { return size_; }
  const char* key() const { return key_; }
  const void* value() const { return value_ptr_; }

 protected:
  friend class AnnotationList;

  std::atomic<Annotation*>& link_node() { return link_node_; }

 private:
  //! \brief Linked list next-node pointer. Accessed only by \sa AnnotationList.
  //!
  //! This will be null until the first call to \sa SetSze(), after which the
  //! presence of the pointer will prevent the node from being added to the
  //! list again.
  std::atomic<Annotation*> link_node_;

  const char* const key_;
  void* const value_ptr_;
  uint32_t size_;
  const Type type_;

  DISALLOW_COPY_AND_ASSIGN(Annotation);
};

//! \brief An \sa Annotation that stores a `NUL`-terminated C-string value.
//!
//! The storage for the value is allocated by the annotation and the template
//! parameter \a MaxSize controls the maxmium length for the value.
template <uint32_t MaxSize>
class StringAnnotation : public Annotation {
 public:
  //! \brief Constructs a new StringAnnotation with the given \a key.
  //!
  //! \param[in] key The Annotation key.
  constexpr explicit StringAnnotation(const char key[])
      : Annotation(Type::kString, key, value_) {}

  //! \brief Sets the Annotation's string value.
  //!
  //! \param[in] value The `NUL`-terminated C-string value.
  void Set(const char* value) {
    size_t n = base::strlcpy(value_, value, MaxSize);
    SetSize(std::min(MaxSize, static_cast<uint32_t>(n) + 1));
  }

 private:
  char value_[MaxSize];

  DISALLOW_COPY_AND_ASSIGN(StringAnnotation);
};

}  // namespace crashpad

#endif  // CRASHPAD_CLIENT_ANNOTATION_H_
