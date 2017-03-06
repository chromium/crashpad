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

#ifndef CRASHPAD_CLIENT_ANNOTATIONS_H_
#define CRASHPAD_CLIENT_ANNOTATIONS_H_

#include "build/build_config.h"
#include "base/atomicops.h"
#include "base/logging.h"

#if defined(OS_MACOSX)
#include <mach-o/loader.h>
#endif

namespace crashpad {

namespace internal {

//! \brief The data structure that is defined in a global variable for each
//!     annotation.
struct RawAnnotation {
  //! \brief The key name for the annotation.
  //!
  //! This field is only ever set once during initialization in
  //! `CRASHPAD_DEFINE_ANNOTATION_*()`.
  const char* const key_name_;

  //! \brief A pointer (which must be written atomically) that points the the
  //!     value for the annotation.
  base::subtle::AtomicWord data_;

  //! \brief The type of data that data_ points to. Currently only strings are
  //!     supported.
  enum Type : uint32_t {
    //! \brief Pointer to unowned UTF-8 string.
    kString = 0,

    //! \brief Pointer to UTF-8 string for which the annotation owns the
    //!     lifetime.
    kStringOwned = 1,
  };

  //! \brief The type of data which data_ points to. Currently always a string.
  //! 
  //! This field is only ever set once during initialization in
  //! `CRASHPAD_DEFINE_ANNOTATION_*()`.
  const Type type_;

#if defined(OS_WIN)
  //! \brief Padding used to make these elements pack as expected when combined
  //!     into a section on Windows.
  uint32_t padding0;
#if defined(ARCH_CPU_X86_X64)
  //! \brief Padding used to make these elements pack as expected when combined
  //!     into a section on Windows.
  uint64_t padding1;
#endif  // ARCH_CPU_X86_X64
#endif  // OS_WIN
};

//! \brief Internal implementation helper for
//!     CRASHPAD_SET_ANNOTATION_STRING_COPIED().
void SetAnnotationStringCopied(RawAnnotation* annotation, const char* value);

}  // namespace internal

#if defined(OS_WIN) || DOXYGEN

#pragma section("CPADanot", read, write)

//! \brief Internal-only helper macro to define an annotation with the given
//!     name and type.
#define CRASHPAD_DEFINE_ANNOTATION_IMPL(key_name, type)                        \
  __declspec(allocate("CPADanot")) __declspec(selectany) __declspec(align(16)) \
      crashpad::internal::RawAnnotation g_crashpad_annotation_##key_name = {   \
          #key_name, 0, crashpad::internal::RawAnnotation::Type::type};

#elif defined(OS_MACOSX)

// TODO(scottmg): COMDAT
#define CRASHPAD_DEFINE_ANNOTATION_IMPL(key_name, type)                      \
  __attribute__(                                                             \
      (section(SEG_DATA ",__crashpad_anot"), used, visibility("hidden")))    \
      crashpad::internal::RawAnnotation g_crashpad_annotation_##key_name = { \
          #key_name, 0, crashpad::internal::RawAnnotation::Type::type}

#endif  // OS_WIN || DOXYGEN

//! \brief Defines an annotation with a value type of constant string.
//!
//! This macro must be used at global scope in .cc files.
#define CRASHPAD_DEFINE_ANNOTATION_STRING_CONSTANT(key_name) \
  CRASHPAD_DEFINE_ANNOTATION_IMPL(key_name, kString)

//! \brief Defines an annotation with a value type of string, where the string
//!     value is copied and owned by the annotation system.
//!
//! This macro must be used at global scope in .cc files.
#define CRASHPAD_DEFINE_ANNOTATION_STRING_COPIED(key_name) \
  CRASHPAD_DEFINE_ANNOTATION_IMPL(key_name, kStringOwned)

//! \brief Sets the value of an annotation previously defined by
//!     CRASHPAD_DEFINE_ANNOTATION_STRING_CONSTANT(). Ownership of the string
//!     pointer is not taken, so common usage is for \a value to be a string
//!     constant.
#define CRASHPAD_SET_ANNOTATION_STRING_CONSTANT(key_name, value) \
  do {                                                           \
    DCHECK_EQ(::g_crashpad_annotation_##key_name.type_,          \
              crashpad::internal::RawAnnotation::Type::kString); \
    base::subtle::Release_Store(                                 \
        &::g_crashpad_annotation_##key_name.data_,               \
        reinterpret_cast<base::subtle::AtomicWord>(value));      \
  } while (false)

//! \brief Sets the value of an annotation previously defined by
//!     CRASHPAD_DEFINE_ANNOTATION_STRING_COPIED(). The string value is copied
//!     and the copy is owned by the annotation system.
#define CRASHPAD_SET_ANNOTATION_STRING_COPIED(key_name, value) \
  crashpad::internal::SetAnnotationStringCopied(               \
      &g_crashpad_annotation_##key_name, value)

}  // namespace crashpad

#endif  // CRASHPAD_CLIENT_ANNOTATIONS_H_
