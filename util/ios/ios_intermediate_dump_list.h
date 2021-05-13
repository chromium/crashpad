// Copyright 2021 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_UTIL_IOS_IOS_INTERMEDIATEDUMP_LIST_H_
#define CRASHPAD_UTIL_IOS_IOS_INTERMEDIATEDUMP_LIST_H_

#include <vector>

#include "util/ios/ios_intermediate_dump_map.h"
#include "util/ios/ios_intermediate_dump_object.h"

namespace crashpad {
namespace internal {

//! \brief A list object, consisting of a vector of IOSIntermediateDumpMap.
//!
//! Provides a wrapper around an internal std::vector.
class IOSIntermediateDumpList : public IOSIntermediateDumpObject {
 public:
  IOSIntermediateDumpList() : list_() {}
  // IOSIntermediateDumpObject:
  Type type() const override { return Type::kList; }

  using VectorType = std::vector<std::unique_ptr<const IOSIntermediateDumpMap>>;
  VectorType::const_iterator begin() const { return list_.begin(); }
  VectorType::const_iterator end() const { return list_.end(); }
  VectorType::size_type size() const { return list_.size(); }
  bool empty() const { return list_.empty(); }
  VectorType::const_reference operator[](VectorType::size_type index) const {
    return list_[index];
  }
  void push_back(std::unique_ptr<const IOSIntermediateDumpMap> val) {
    list_.push_back(std::move(val));
  }

 private:
  VectorType list_;

  DISALLOW_COPY_AND_ASSIGN(IOSIntermediateDumpList);
};

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_UTIL_IOS_IOS_INTERMEDIATEDUMP_LIST_H_
