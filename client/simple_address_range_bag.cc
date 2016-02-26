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

#include "client/simple_address_range_bag.h"

#include "util/stdlib/cxx.h"

#if CXX_LIBRARY_VERSION >= 2011
#include <type_traits>
#endif

namespace crashpad {
namespace {

using SimpleAddressRangeBagForAssertion = TSimpleAddressRangeBag<1>;

#if CXX_LIBRARY_VERSION >= 2011
// In C++11, check that TSimpleAddressRangeBag has standard layout, which is
// what is actually important.
static_assert(
    std::is_standard_layout<SimpleAddressRangeBagForAssertion>::value,
    "SimpleAddressRangeBag must be standard layout");
#else
// In C++98 (ISO 14882), section 9.5.1 says that a union cannot have a member
// with a non-trivial ctor, copy ctor, dtor, or assignment operator. Use this
// property to ensure that Entry remains POD. This doesn’t work for C++11
// because the requirements for unions have been relaxed.
union Compile_Assert {
  SimpleAddressRangeBagForAssertion::Entry Compile_Assert__entry_must_be_pod;
};
#endif

}  // namespace
}  // namespace crashpad
