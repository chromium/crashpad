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

#ifndef CRASHPAD_UTIL_IOS_IOS_INTERMEDIATEDUMP_DATA_H_
#define CRASHPAD_UTIL_IOS_IOS_INTERMEDIATEDUMP_DATA_H_

#include <string>

#include "util/ios/ios_intermediatedump_object.h"

namespace crashpad {
namespace internal {

//! \brief A data object, consisting of an array of uint8_t and a length.
class IOSIntermediatedumpData : public IOSIntermediatedumpObject {
 public:
  //! \brief Constructs a new data object.
  //!
  //! \param[in] data An array of uint8_t.
  //! \param[in] length The length of \a data.
  IOSIntermediatedumpData(std::unique_ptr<uint8_t[]> data, uint64_t length)
      : data_(std::move(data)), length_(length) {}

  // IOSIntermediatedumpObject:
  IOSIntermediatedumpObjectType type() const override { return DATA; }
  const IOSIntermediatedumpData& AsData() const override { return *this; }
  void GetString(std::string* string) const override;
  void GetInt(int* val) const override;
  void GetBool(bool* val) const override;
  const uint8_t* bytes() const override;
  uint64_t length() const override;

  //! \brief Copies the data into \a value if sizeof(T) matches length_.
  //!
  //! \param[out] value The data to populate.
  //!
  //! \return On success, returns `true`, otherwise returns `false`.
  template <typename T>
  bool GetValue(T* value) const {
    if (sizeof(T) == length_) {
      memcpy(value, data_.get(), length_);
      return true;
    }
    return false;
  }

 private:
  std::unique_ptr<uint8_t[]> data_;
  const uint64_t length_;
};

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_UTIL_IOS_IOS_INTERMEDIATEDUMP_DATA_H_
