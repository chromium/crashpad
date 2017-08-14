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

AnnotationList::AnnotationList()
    : head_(Annotation::Type::kInvalid, nullptr, nullptr),
      tail_(Annotation::Type::kInvalid, nullptr, nullptr) {
  head_.link_node().store(&tail_);
}

AnnotationList::~AnnotationList() {}

// static
AnnotationList* AnnotationList::Get() {
  return CrashpadInfo::GetCrashpadInfo()->annotations_list();
}

void AnnotationList::Add(Annotation* annotation) {
  Annotation* null = nullptr;
  Annotation* head_next = head_.link_node().load(std::memory_order_relaxed);
  if (!annotation->link_node().compare_exchange_strong(null, head_next)) {
    // If |annotation|'s link node is not null, then it has been added to the
    // list already and no work needs to be done.
    return;
  }

  // Update the head link to point to the new |annotation|.
  while (!head_.link_node().compare_exchange_weak(head_next, annotation)) {
    // Another thread has updated the head-next pointer, so try again with the
    // re-loaded |head_next|.
    annotation->link_node().store(head_next, std::memory_order_relaxed);
  }
}

Annotation* AnnotationList::IteratorNext(Annotation* curr) {
  if (curr == nullptr)
    curr = &head_;

  Annotation* next = curr->link_node();
  if (next == &tail_)
    return nullptr;

  return next;
}

}  // namespace crashpad
