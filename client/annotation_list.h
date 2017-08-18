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

#include "base/macros.h"
#include "client/annotation.h"

namespace crashpad {

class AnnotationList;

//! \brief A list that contains all the currently set annotations.
//!
//! An instance of this class must be registered on the \a CrashpadInfo
//! structure in order to use the annotations system. Once a list object has
//! been registered on the CrashpadInfo, a different instance should not
//! be used instead.
class AnnotationList {
 public:
  AnnotationList();
  ~AnnotationList();

  //! \brief Returns the instance of the list that has been registered on the
  //!     \sa CrashapdInfo structure.
  static AnnotationList* Get();

  //! \brief Adds \a annotation to the global list. This method does not need
  //!     to be called by clients directly. The Annotation object will do so
  //!     automatically.
  //!
  //! Once an annotation is added to the list, it is not removed. This is
  //! because the AnnotationList avoids the use of locks/mutexes, in case it is
  //! being manipulated in a compromised context. Instead, an Annotation keeps
  //! track of when it has been cleared, which excludes it from a crash report.
  //! This design also avoids linear scans of the list when repeatedly setting
  //! and/or clearing the value.
  void Add(Annotation* annotation);

  //! \brief Iterates over the annotation list.
  //!
  //! \param[in] curr The current iterator position, which is typically the
  //!     return value from a prior call to this method. To begin iterating the
  //!     list, pass `nullptr`.
  //!
  //! \return The next element in the annotations list, or `nullptr` if there is
  //!     no next element.
  Annotation* IteratorNext(Annotation* curr);

 private:
  // Dummy linked-list head and tail elements of \a Annotation::Type::kInvalid.
  Annotation head_;
  Annotation tail_;

  DISALLOW_COPY_AND_ASSIGN(AnnotationList);
};

}  // namespace crashpad

#endif  // CRASHPAD_CLIENT_ANNOTATION_LIST_H_
