// Copyright 2014 The Crashpad Authors. All rights reserved.
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

// This file is intended to be included multiple times in the same translation
// unit, so #include guards are intentionally absent.
//
// This file is included by util/mac/process_types.h and
// util/mac/process_types/internal.h to produce traits class definitions.

// Things that #include this file should #undef
// DECLARE_PROCESS_TYPE_TRAITS_CLASS before #including this file again and after
// the last #include of this file.
//
// |Reserved| is used for padding fields that may be zero-length, and thus
// __VA_ARGS__, which is intended to set the alignment of the 64-bit types, is
// not used for that typedef.
#define DECLARE_PROCESS_TYPE_TRAITS_CLASS(traits_name, lp_bits, ...) \
  namespace crashpad {                                                  \
  namespace process_types {                                          \
  namespace internal {                                               \
  struct Traits##traits_name {                                       \
    typedef int##lp_bits##_t Long __VA_ARGS__;                       \
    typedef uint##lp_bits##_t ULong __VA_ARGS__;                     \
    typedef uint##lp_bits##_t Pointer __VA_ARGS__;                   \
    typedef int##lp_bits##_t IntPtr __VA_ARGS__;                     \
    typedef uint##lp_bits##_t UIntPtr __VA_ARGS__;                   \
    typedef Reserved64Only##lp_bits Reserved64Only;                  \
  };                                                                 \
  }                                                                  \
  }                                                                  \
  }
