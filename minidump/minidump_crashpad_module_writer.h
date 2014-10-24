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

#ifndef CRASHPAD_MINIDUMP_MINIDUMP_CRASHPAD_MODULE_WRITER_H_
#define CRASHPAD_MINIDUMP_MINIDUMP_CRASHPAD_MODULE_WRITER_H_

#include <stdint.h>

#include <vector>

#include "base/basictypes.h"
#include "minidump/minidump_extensions.h"
#include "minidump/minidump_writable.h"

namespace crashpad {

class MinidumpSimpleStringDictionaryWriter;

//! \brief The writer for a MinidumpModuleCrashpadInfo object in a minidump
//!     file.
class MinidumpModuleCrashpadInfoWriter final
    : public internal::MinidumpWritable {
 public:
  MinidumpModuleCrashpadInfoWriter();
  ~MinidumpModuleCrashpadInfoWriter();

  //! \brief Sets MinidumpModuleCrashpadInfo::minidump_module_list_index.
  void SetMinidumpModuleListIndex(uint32_t minidump_module_list_index) {
    module_.minidump_module_list_index = minidump_module_list_index;
  }

  //! \brief Arranges for MinidumpModuleCrashpadInfo::simple_annotations to
  //!     point to the MinidumpSimpleStringDictionaryWriter object to be written
  //!     by \a simple_annotations.
  //!
  //! \a simple_annotations will become a child of this object in the overall
  //! tree of internal::MinidumpWritable objects.
  //!
  //! \note Valid in #kStateMutable.
  void SetSimpleAnnotations(
      MinidumpSimpleStringDictionaryWriter* simple_annotations);

 protected:
  // MinidumpWritable:
  bool Freeze() override;
  size_t SizeOfObject() override;
  std::vector<MinidumpWritable*> Children() override;
  bool WriteObject(FileWriterInterface* file_writer) override;

 private:
  MinidumpModuleCrashpadInfo module_;
  MinidumpSimpleStringDictionaryWriter* simple_annotations_;  // weak

  DISALLOW_COPY_AND_ASSIGN(MinidumpModuleCrashpadInfoWriter);
};

//! \brief The writer for a MinidumpModuleCrashpadInfoList object in a minidump
//!     file, containing a list of MinidumpModuleCrashpadInfo objects.
class MinidumpModuleCrashpadInfoListWriter final
    : public internal::MinidumpWritable {
 public:
  MinidumpModuleCrashpadInfoListWriter();
  ~MinidumpModuleCrashpadInfoListWriter();

  //! \brief Adds a MinidumpModuleCrashpadInfo to the
  //!     MinidumpModuleCrashpadInfoList.
  //!
  //! \a module will become a child of this object in the overall tree of
  //!     internal::MinidumpWritable objects.
  //!
  //! \note Valid in #kStateMutable.
  void AddModule(MinidumpModuleCrashpadInfoWriter* module);

 protected:
  // MinidumpWritable:
  bool Freeze() override;
  size_t SizeOfObject() override;
  std::vector<MinidumpWritable*> Children() override;
  bool WriteObject(FileWriterInterface* file_writer) override;

 private:
  MinidumpModuleCrashpadInfoList module_list_base_;
  std::vector<MinidumpModuleCrashpadInfoWriter*> modules_;  // weak
  std::vector<MINIDUMP_LOCATION_DESCRIPTOR> module_location_descriptors_;

  DISALLOW_COPY_AND_ASSIGN(MinidumpModuleCrashpadInfoListWriter);
};

}  // namespace crashpad

#endif  // CRASHPAD_MINIDUMP_MINIDUMP_CRASHPAD_MODULE_WRITER_H_
