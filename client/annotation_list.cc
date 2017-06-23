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

#include "client/annotation_list.h"

#include "client/crashpad_info.h"

namespace crashpad {

namespace internal {

void Annotation::AddToList() {
  if (!is_set_.exchange(true)) {
    CrashpadInfo::GetCrashpadInfo()->annotations_list()->Add(this);
  }
}

void Annotation::RemoveFromList() {
  CrashpadInfo::GetCrashpadInfo()->annotations_list()->Remove(this);
  is_set_.exchange(false);
}

}  // namespace internal

AnnotationList::AnnotationList() : head_() {}

AnnotationList::~AnnotationList() {}

void AnnotationList::Add(internal::Annotation* annotation) {
  base::AutoLock lock(lock_);

  for (internal::Annotation* curr = head_.link_node();
       curr != nullptr;
       curr = curr->link_node()) {
    if (curr == annotation) {
      return;
    }
  }

  internal::Annotation* curr = head_.link_node();
  annotation->set_link_node(curr);
  head_.set_link_node(annotation);
}

void AnnotationList::Remove(internal::Annotation* annotation) {
  base::AutoLock lock(lock_);
  internal::Annotation* prev = &head_;
  for (internal::Annotation* curr = head_.link_node();
       curr != nullptr;
       curr = curr->link_node()) {
    if (curr == annotation) {
      prev->set_link_node(curr->link_node());
      curr->set_link_node(nullptr);
      break;
    } else {
      prev = curr;
    }
  }
}

internal::Annotation* AnnotationList::IteratorNext(internal::Annotation* curr) {
  if (curr == nullptr)
    curr = &head_;
  return curr->link_node();
}

}  // namespace crashpad
