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

#ifndef CRASHPAD_CLIENT_ANNOTATION_LIST_H_
#define CRASHPAD_CLIENT_ANNOTATION_LIST_H_

#include <atomic>

#include "base/macros.h"
#include "base/synchronization/lock.h"

namespace crashpad {

class AnnotationList;

namespace internal {

//! \brief Base class for an annotation.
//!
//! An annotation records an a key-value pair of an arbitrary type. Annotations
//! should be declared in the file in which they are used.
class Annotation {
 public:
  //! \brief The type of data stored in the annotation.
  enum class Type : uint32_t {
    //! \brief An invalid annotation. Reserved for internal use.
    kInvalid = 0,

    //! \brief A nul-terminated C-string.
    kString = 1,

    //! \brief Clients may declare their own custom types by using values greater
    //!     than this.
    kUserDefinedStart = 0xFFFF,
  };

  //! \brief Constructs a new annotation.
  constexpr Annotation(Type type, const char key[], char* const value_ptr)
      : key_(key),
        value_ptr_(value_ptr),
        link_node_(),
        type_(type),
        is_set_() {}

  Type type() const { return type_; }
  const char* key() const { return key_; }
  const char* value() const { return value_ptr_; }

 protected:
  void AddToList();
  void RemoveFromList();

 private:
  friend class crashpad::AnnotationList;

  Annotation* link_node() { return link_node_; }
  void set_link_node(Annotation* link_node) { link_node_ = link_node; }

  const char* const key_;
  char* const value_ptr_;
  Annotation* link_node_;

  const Type type_;

  //! \brief A flag used to determine if the Annotation has already been added to
  //!     global list.
  //!
  //! Because \a AddToList() requires taking a global lock, this flag helps avoid
  //! lock contention by only calling \a AnnotationList::Add if this has not
  //! already been added to the list.
  std::atomic_bool is_set_;

  DISALLOW_COPY_AND_ASSIGN(Annotation);
};

//! \brief An invalid annotation type used to represent the head of the
//!     annotations linked list.
class SentinelLink : public internal::Annotation {
 public:
  SentinelLink() : internal::Annotation(Type::kInvalid, nullptr, nullptr) {}
  ~SentinelLink() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SentinelLink);
};

}  // namespace internal

//! \brief A list that contains all the currently set annotations.
//!
//! An instance of this class must be registered on the \a CrashpadInfo structure
//! in order to use the annotations system.
class AnnotationList {
 public:
  AnnotationList();
  ~AnnotationList();

  void Add(internal::Annotation* annotation);
  void Remove(internal::Annotation* annotation);

  //! \brief Iterates over the annotation list.
  //!
  //! \param[in] curr The curret iterator position, which is typically the return
  //!     value from a prior call to this method. To begin iterating the list,
  //!     pass `nullptr`.
  //!
  //! \return The next element in the annotations list, or `nullptr` if there is
  //!     no next element.
  internal::Annotation* IteratorNext(internal::Annotation* curr);

 private:
  base::Lock lock_;
  internal::SentinelLink head_;

  DISALLOW_COPY_AND_ASSIGN(AnnotationList);
};

}  // namespace crashpad

#endif  // CRASHPAD_CLIENT_ANNOTATION_LIST_H_
