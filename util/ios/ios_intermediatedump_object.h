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

#ifndef CRASHPAD_UTIL_IOS_IOS_INTERMEDIATEDUMP_OBJECT_H_
#define CRASHPAD_UTIL_IOS_IOS_INTERMEDIATEDUMP_OBJECT_H_

#include "util/ios/ios_intermediatedump_writer.h"

namespace crashpad {
namespace internal {

class IOSIntermediatedumpMap;
class IOSIntermediatedumpData;
class IOSIntermediatedumpList;

//! \brief Base class for intermediate dump object types.
class IOSIntermediatedumpObject {
 public:
  virtual ~IOSIntermediatedumpObject() {}

  //! \brief The type of object stored in the intermediate dump.  .
  enum IOSIntermediatedumpObjectType {
    //! \brief A data object, containing array of bytes.
    DATA,

    //! \brief A map object, containing other lists, maps and data objects.
    MAP,

    //! \brief A list object, containing a list of map objects.
    LIST,
  };

  //! \brief Returns an IOSIntermediatedumpData.
  virtual IOSIntermediatedumpObjectType type() const = 0;

  //! \brief Returns an IOSIntermediatedumpData. If the type is not DATA,
  //!     returns an empty IOSIntermediatedumpData object
  virtual const IOSIntermediatedumpData& AsData() const;

  //! \brief Returns an IOSIntermediatedumpList. If the type is not LIST,
  //!     returns an empty IOSIntermediatedumpList object
  virtual const IOSIntermediatedumpList& AsList() const;

  //! \brief Returns an IOSIntermediatedumpMap. If the type is not MAP,
  //!     returns an empty IOSIntermediatedumpMap object
  virtual const IOSIntermediatedumpMap& AsMap() const;

  //! \brief Helper subscript operator to allow calling [] directly on an object
  //!     without first calling AsMap().
  virtual const IOSIntermediatedumpObject& operator[](
      const IntermediateDumpKey& key) const {
    return *this;
  }

  //! \brief Helper to allow calling HasKey directly on an object
  //!     without first calling AsMap().
  virtual bool HasKey(const IntermediateDumpKey& key) const { return false; }

  //! \brief Helper to allow calling GetString directly on an object
  //!     without first calling AsData().
  virtual bool GetString(std::string* string) const { return false; }

  //! \brief Helper to allow calling GetInt directly on an object
  //!     without first calling AsData().
  virtual bool GetInt(int* num) const { return false; }

  //! \brief Helper to allow calling GetBool directly on an object
  //!     without first calling AsData().
  virtual bool GetBool(bool* val) const { return false; }

  //! \brief Helper to allow calling bytes directly on an object
  //!     without first calling AsData().
  virtual const uint8_t* bytes() const { return nullptr; }

  //! \brief Helper to allow calling length directly on an object
  //!     without first calling AsData().
  virtual uint64_t length() const { return 0; }

  //! \brief Type of object.
  IOSIntermediatedumpObjectType type_;
};

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_UTIL_IOS_IOS_INTERMEDIATEDUMP_OBJECT_H_
