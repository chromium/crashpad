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

#include "minidump/minidump_module_writer.h"

#include "base/logging.h"
#include "minidump/minidump_string_writer.h"
#include "minidump/minidump_writer_util.h"
#include "util/numeric/safe_assignment.h"

namespace crashpad {

MinidumpModuleCodeViewRecordWriter::~MinidumpModuleCodeViewRecordWriter() {
}

namespace internal {

template <typename CodeViewRecordType>
MinidumpModuleCodeViewRecordPDBLinkWriter<
    CodeViewRecordType>::MinidumpModuleCodeViewRecordPDBLinkWriter()
    : MinidumpModuleCodeViewRecordWriter(), codeview_record_(), pdb_name_() {
  codeview_record_.signature = CodeViewRecordType::kSignature;
}

template <typename CodeViewRecordType>
MinidumpModuleCodeViewRecordPDBLinkWriter<
    CodeViewRecordType>::~MinidumpModuleCodeViewRecordPDBLinkWriter() {
}

template <typename CodeViewRecordType>
size_t
MinidumpModuleCodeViewRecordPDBLinkWriter<CodeViewRecordType>::SizeOfObject() {
  DCHECK_GE(state(), kStateFrozen);

  // NUL-terminate.
  return offsetof(typeof(codeview_record_), pdb_name) +
         (pdb_name_.size() + 1) * sizeof(pdb_name_[0]);
}

template <typename CodeViewRecordType>
bool MinidumpModuleCodeViewRecordPDBLinkWriter<CodeViewRecordType>::WriteObject(
    FileWriterInterface* file_writer) {
  DCHECK_EQ(state(), kStateWritable);

  WritableIoVec iov;
  iov.iov_base = &codeview_record_;
  iov.iov_len = offsetof(typeof(codeview_record_), pdb_name);
  std::vector<WritableIoVec> iovecs(1, iov);

  // NUL-terminate.
  iov.iov_base = &pdb_name_[0];
  iov.iov_len = (pdb_name_.size() + 1) * sizeof(pdb_name_[0]);
  iovecs.push_back(iov);

  return file_writer->WriteIoVec(&iovecs);
}

}  // namespace internal

template class internal::MinidumpModuleCodeViewRecordPDBLinkWriter<
    MinidumpModuleCodeViewRecordPDB20>;

MinidumpModuleCodeViewRecordPDB20Writer::
    ~MinidumpModuleCodeViewRecordPDB20Writer() {
}

void MinidumpModuleCodeViewRecordPDB20Writer::SetTimestampAndAge(
    time_t timestamp,
    uint32_t age) {
  DCHECK_EQ(state(), kStateMutable);

  internal::MinidumpWriterUtil::AssignTimeT(&codeview_record()->timestamp,
                                            timestamp);

  codeview_record()->age = age;
}

template class internal::MinidumpModuleCodeViewRecordPDBLinkWriter<
    MinidumpModuleCodeViewRecordPDB70>;

MinidumpModuleCodeViewRecordPDB70Writer::
    ~MinidumpModuleCodeViewRecordPDB70Writer() {
}

MinidumpModuleMiscDebugRecordWriter::MinidumpModuleMiscDebugRecordWriter()
    : internal::MinidumpWritable(),
      image_debug_misc_(),
      data_(),
      data_utf16_() {
}

void MinidumpModuleMiscDebugRecordWriter::SetData(const std::string& data,
                                                  bool utf16) {
  DCHECK_EQ(state(), kStateMutable);

  if (!utf16) {
    data_utf16_.clear();
    image_debug_misc_.Unicode = 0;
    data_ = data;
  } else {
    data_.clear();
    image_debug_misc_.Unicode = 1;
    data_utf16_ = internal::MinidumpWriterUtil::ConvertUTF8ToUTF16(data);
  }
}

bool MinidumpModuleMiscDebugRecordWriter::Freeze() {
  DCHECK_EQ(state(), kStateMutable);

  if (!MinidumpWritable::Freeze()) {
    return false;
  }

  // NUL-terminate.
  if (!image_debug_misc_.Unicode) {
    DCHECK(data_utf16_.empty());
    image_debug_misc_.Length = offsetof(typeof(image_debug_misc_), Data) +
                               (data_.size() + 1) * sizeof(data_[0]);
  } else {
    DCHECK(data_.empty());
    image_debug_misc_.Length =
        offsetof(typeof(image_debug_misc_), Data) +
        (data_utf16_.size() + 1) * sizeof(data_utf16_[0]);
  }

  return true;
}

size_t MinidumpModuleMiscDebugRecordWriter::SizeOfObject() {
  DCHECK_GE(state(), kStateFrozen);

  return image_debug_misc_.Length;
}

bool MinidumpModuleMiscDebugRecordWriter::WriteObject(
    FileWriterInterface* file_writer) {
  DCHECK_EQ(state(), kStateWritable);

  const size_t base_length = offsetof(typeof(image_debug_misc_), Data);

  WritableIoVec iov;
  iov.iov_base = &image_debug_misc_;
  iov.iov_len = base_length;
  std::vector<WritableIoVec> iovecs(1, iov);

  if (!image_debug_misc_.Unicode) {
    DCHECK(data_utf16_.empty());
    iov.iov_base = &data_[0];
  } else {
    DCHECK(data_.empty());
    iov.iov_base = &data_utf16_[0];
  }
  iov.iov_len = image_debug_misc_.Length - base_length;
  iovecs.push_back(iov);

  return file_writer->WriteIoVec(&iovecs);
}

MinidumpModuleWriter::MinidumpModuleWriter()
    : MinidumpWritable(),
      module_(),
      name_(),
      codeview_record_(nullptr),
      misc_debug_record_(nullptr) {
  module_.VersionInfo.dwSignature = VS_FFI_SIGNATURE;
  module_.VersionInfo.dwStrucVersion = VS_FFI_STRUCVERSION;
}

MinidumpModuleWriter::~MinidumpModuleWriter() {
}

const MINIDUMP_MODULE* MinidumpModuleWriter::MinidumpModule() const {
  DCHECK_EQ(state(), kStateWritable);

  return &module_;
}

void MinidumpModuleWriter::SetName(const std::string& name) {
  DCHECK_EQ(state(), kStateMutable);

  if (!name_) {
    name_.reset(new internal::MinidumpUTF16StringWriter());
  }
  name_->SetUTF8(name);
}

void MinidumpModuleWriter::SetCodeViewRecord(
    MinidumpModuleCodeViewRecordWriter* codeview_record) {
  DCHECK_EQ(state(), kStateMutable);

  codeview_record_ = codeview_record;
}

void MinidumpModuleWriter::SetMiscDebugRecord(
    MinidumpModuleMiscDebugRecordWriter* misc_debug_record) {
  DCHECK_EQ(state(), kStateMutable);

  misc_debug_record_ = misc_debug_record;
}

void MinidumpModuleWriter::SetTimestamp(time_t timestamp) {
  DCHECK_EQ(state(), kStateMutable);

  internal::MinidumpWriterUtil::AssignTimeT(&module_.TimeDateStamp, timestamp);
}

void MinidumpModuleWriter::SetFileVersion(uint16_t version_0,
                                          uint16_t version_1,
                                          uint16_t version_2,
                                          uint16_t version_3) {
  DCHECK_EQ(state(), kStateMutable);

  module_.VersionInfo.dwFileVersionMS =
      (static_cast<uint32_t>(version_0) << 16) | version_1;
  module_.VersionInfo.dwFileVersionLS =
      (static_cast<uint32_t>(version_2) << 16) | version_3;
}

void MinidumpModuleWriter::SetProductVersion(uint16_t version_0,
                                             uint16_t version_1,
                                             uint16_t version_2,
                                             uint16_t version_3) {
  DCHECK_EQ(state(), kStateMutable);

  module_.VersionInfo.dwProductVersionMS =
      (static_cast<uint32_t>(version_0) << 16) | version_1;
  module_.VersionInfo.dwProductVersionLS =
      (static_cast<uint32_t>(version_2) << 16) | version_3;
}

void MinidumpModuleWriter::SetFileFlagsAndMask(uint32_t file_flags,
                                               uint32_t file_flags_mask) {
  DCHECK_EQ(state(), kStateMutable);
  DCHECK_EQ(file_flags & ~file_flags_mask, 0u);

  module_.VersionInfo.dwFileFlags = file_flags;
  module_.VersionInfo.dwFileFlagsMask = file_flags_mask;
}

bool MinidumpModuleWriter::Freeze() {
  DCHECK_EQ(state(), kStateMutable);
  CHECK(name_);

  if (!MinidumpWritable::Freeze()) {
    return false;
  }

  name_->RegisterRVA(&module_.ModuleNameRva);

  if (codeview_record_) {
    codeview_record_->RegisterLocationDescriptor(&module_.CvRecord);
  }

  if (misc_debug_record_) {
    misc_debug_record_->RegisterLocationDescriptor(&module_.MiscRecord);
  }

  return true;
}

size_t MinidumpModuleWriter::SizeOfObject() {
  DCHECK_GE(state(), kStateFrozen);

  // This object doesn’t directly write anything itself. Its MINIDUMP_MODULE is
  // written by its parent as part of a MINIDUMP_MODULE_LIST, and its children
  // are responsible for writing themselves.
  return 0;
}

std::vector<internal::MinidumpWritable*> MinidumpModuleWriter::Children() {
  DCHECK_GE(state(), kStateFrozen);
  DCHECK(name_);

  std::vector<MinidumpWritable*> children;
  children.push_back(name_.get());
  if (codeview_record_) {
    children.push_back(codeview_record_);
  }
  if (misc_debug_record_) {
    children.push_back(misc_debug_record_);
  }

  return children;
}

bool MinidumpModuleWriter::WriteObject(FileWriterInterface* file_writer) {
  DCHECK_EQ(state(), kStateWritable);

  // This object doesn’t directly write anything itself. Its MINIDUMP_MODULE is
  // written by its parent as part of a MINIDUMP_MODULE_LIST, and its children
  // are responsible for writing themselves.
  return true;
}

MinidumpModuleListWriter::MinidumpModuleListWriter()
    : MinidumpStreamWriter(), module_list_base_(), modules_() {
}

MinidumpModuleListWriter::~MinidumpModuleListWriter() {
}

void MinidumpModuleListWriter::AddModule(MinidumpModuleWriter* module) {
  DCHECK_EQ(state(), kStateMutable);

  modules_.push_back(module);
}

bool MinidumpModuleListWriter::Freeze() {
  DCHECK_EQ(state(), kStateMutable);

  if (!MinidumpStreamWriter::Freeze()) {
    return false;
  }

  size_t module_count = modules_.size();
  if (!AssignIfInRange(&module_list_base_.NumberOfModules, module_count)) {
    LOG(ERROR) << "module_count " << module_count << " out of range";
    return false;
  }

  return true;
}

size_t MinidumpModuleListWriter::SizeOfObject() {
  DCHECK_GE(state(), kStateFrozen);

  return sizeof(module_list_base_) + modules_.size() * sizeof(MINIDUMP_MODULE);
}

std::vector<internal::MinidumpWritable*> MinidumpModuleListWriter::Children() {
  DCHECK_GE(state(), kStateFrozen);

  std::vector<MinidumpWritable*> children;
  for (MinidumpModuleWriter* module : modules_) {
    children.push_back(module);
  }

  return children;
}

bool MinidumpModuleListWriter::WriteObject(FileWriterInterface* file_writer) {
  DCHECK_EQ(state(), kStateWritable);

  WritableIoVec iov;
  iov.iov_base = &module_list_base_;
  iov.iov_len = sizeof(module_list_base_);
  std::vector<WritableIoVec> iovecs(1, iov);

  for (const MinidumpModuleWriter* module : modules_) {
    iov.iov_base = module->MinidumpModule();
    iov.iov_len = sizeof(MINIDUMP_MODULE);
    iovecs.push_back(iov);
  }

  return file_writer->WriteIoVec(&iovecs);
}

MinidumpStreamType MinidumpModuleListWriter::StreamType() const {
  return kMinidumpStreamTypeModuleList;
}

}  // namespace crashpad
