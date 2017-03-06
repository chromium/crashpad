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

#include "client/annotations.h"

#include <stdlib.h>
#include <string.h>

#include "base/logging.h"

namespace crashpad {
namespace internal {

void SetAnnotationStringCopied(RawAnnotation* annotation, const char* value) {
  DCHECK_EQ(annotation->type_, RawAnnotation::Type::kStringOwned);
  // We want the handler to have a consistent view of the string value. So,
  // first, duplicate the string we're going to store, then atomically swap it
  // into the data_ field, and finally free the old pointer.
  const char* value_copy = strdup(value);

  const char* old_value =
      reinterpret_cast<const char*>(base::subtle::NoBarrier_AtomicExchange(
          &annotation->data_,
          reinterpret_cast<base::subtle::AtomicWord>(value_copy)));

  free(const_cast<void*>(reinterpret_cast<const void*>(old_value)));
}

}  // namespace internal
}  // namespace crashpad
