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

#ifndef CRASHPAD_MINIDUMP_MINIDUMP_STRING_WRITER_H_
#define CRASHPAD_MINIDUMP_MINIDUMP_STRING_WRITER_H_

#include <dbghelp.h>
#include <stdint.h>
#include <sys/types.h>

#include <string>

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "minidump/minidump_extensions.h"
#include "minidump/minidump_writable.h"
#include "util/file/file_writer.h"

namespace crashpad {
namespace internal {

//! \cond

struct MinidumpStringWriterUTF16Traits {
  typedef string16 StringType;
  typedef MINIDUMP_STRING MinidumpStringType;
};

struct MinidumpStringWriterUTF8Traits {
  typedef std::string StringType;
  typedef MinidumpUTF8String MinidumpStringType;
};

//! \endcond

//! \brief Writes a variable-length string to a minidump file in accordance with
//!     the string type’s characteristics.
//!
//! MinidumpStringWriter objects should not be instantiated directly. To write
//! strings to minidump file, use the MinidumpUTF16StringWriter and
//! MinidumpUTF8StringWriter subclasses instead.
template <typename Traits>
class MinidumpStringWriter : public MinidumpWritable {
 public:
  MinidumpStringWriter();
  ~MinidumpStringWriter();

 protected:
  typedef typename Traits::MinidumpStringType MinidumpStringType;
  typedef typename Traits::StringType StringType;

  bool Freeze() override;
  size_t SizeOfObject() override;
  bool WriteObject(FileWriterInterface* file_writer) override;

  //! \brief Sets the string to be written.
  //!
  //! \note Valid in #kStateMutable.
  void set_string(const StringType& string) { string_.assign(string); }

  //! \brief Retrieves the string to be written.
  //!
  //! \note Valid in any state.
  const StringType& string() const { return string_; }

 private:
  MinidumpStringType string_base_;
  StringType string_;

  DISALLOW_COPY_AND_ASSIGN(MinidumpStringWriter);
};

//! \brief Writes a variable-length UTF-16-encoded MINIDUMP_STRING to a minidump
//!     file.
//!
//! MinidumpUTF16StringWriter objects should not be instantiated directly
//! outside of the MinidumpWritable family of classes.
class MinidumpUTF16StringWriter final
    : public MinidumpStringWriter<MinidumpStringWriterUTF16Traits> {
 public:
  MinidumpUTF16StringWriter() : MinidumpStringWriter() {}
  ~MinidumpUTF16StringWriter() {}

  //! \brief Converts a UTF-8 string to UTF-16 and sets it as the string to be
  //!     written.
  //!
  //! \note Valid in #kStateMutable.
  void SetUTF8(const std::string& string_utf8);

 private:
  DISALLOW_COPY_AND_ASSIGN(MinidumpUTF16StringWriter);
};

//! \brief Writes a variable-length UTF-8-encoded MinidumpUTF8String to a
//!     minidump file.
//!
//! MinidumpUTF8StringWriter objects should not be instantiated directly outside
//! of the MinidumpWritable family of classes.
class MinidumpUTF8StringWriter final
    : public MinidumpStringWriter<MinidumpStringWriterUTF8Traits> {
 public:
  MinidumpUTF8StringWriter() : MinidumpStringWriter() {}
  ~MinidumpUTF8StringWriter() {}

  //! \brief Sets the string to be written.
  //!
  //! \note Valid in #kStateMutable.
  void SetUTF8(const std::string& string_utf8) { set_string(string_utf8); }

  //! \brief Retrieves the string to be written.
  //!
  //! \note Valid in any state.
  const std::string& UTF8() const { return string(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(MinidumpUTF8StringWriter);
};

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_MINIDUMP_MINIDUMP_STRING_WRITER_H_
