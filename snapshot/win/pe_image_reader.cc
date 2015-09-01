// Copyright 2015 The Crashpad Authors. All rights reserved.
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

#include "snapshot/win/pe_image_reader.h"

#include <string.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "client/crashpad_info.h"
#include "snapshot/win/process_reader_win.h"
#include "util/misc/pdb_structures.h"

namespace crashpad {

namespace {

std::string RangeToString(const CheckedWinAddressRange& range) {
  return base::StringPrintf("[0x%llx + 0x%llx (%s)]",
                            range.Base(),
                            range.Size(),
                            range.Is64Bit() ? "64" : "32");
}

}  // namespace

PEImageReader::PEImageReader()
    : process_reader_(nullptr),
      module_range_(),
      module_name_(),
      initialized_() {
}

PEImageReader::~PEImageReader() {
}

bool PEImageReader::Initialize(ProcessReaderWin* process_reader,
                               WinVMAddress address,
                               WinVMSize size,
                               const std::string& module_name) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  process_reader_ = process_reader;
  module_range_.SetRange(process_reader_->Is64Bit(), address, size);
  if (!module_range_.IsValid()) {
    LOG(WARNING) << "invalid module range for " << module_name << ": "
                 << RangeToString(module_range_);
    return false;
  }
  module_name_ = module_name;

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

bool PEImageReader::GetCrashpadInfo(
    process_types::CrashpadInfo* crashpad_info) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  IMAGE_SECTION_HEADER section;
  if (!GetSectionByName("CPADinfo", &section))
    return false;

  if (section.Misc.VirtualSize < sizeof(process_types::CrashpadInfo)) {
    LOG(WARNING) << "small crashpad info section size "
                 << section.Misc.VirtualSize << ", " << module_name_;
    return false;
  }

  WinVMAddress crashpad_info_address = Address() + section.VirtualAddress;
  CheckedWinAddressRange crashpad_info_range(process_reader_->Is64Bit(),
                                             crashpad_info_address,
                                             section.Misc.VirtualSize);
  if (!crashpad_info_range.IsValid()) {
    LOG(WARNING) << "invalid range for crashpad info: "
                 << RangeToString(crashpad_info_range);
    return false;
  }

  if (!module_range_.ContainsRange(crashpad_info_range)) {
    LOG(WARNING) << "crashpad info does not fall inside module "
                 << module_name_;
    return false;
  }

  // TODO(scottmg): process_types for cross-bitness.
  if (!process_reader_->ReadMemory(crashpad_info_address,
                                   sizeof(process_types::CrashpadInfo),
                                   crashpad_info)) {
    LOG(WARNING) << "could not read crashpad info " << module_name_;
    return false;
  }

  if (crashpad_info->signature != CrashpadInfo::kSignature ||
      crashpad_info->version < 1) {
    LOG(WARNING) << "unexpected crashpad info data " << module_name_;
    return false;
  }

  return true;
}

bool PEImageReader::DebugDirectoryInformation(UUID* uuid,
                                              DWORD* age,
                                              std::string* pdbname) {
  WinVMAddress nt_headers_address;
  IMAGE_NT_HEADERS nt_headers;
  if (!ReadNtHeaders(&nt_headers_address, &nt_headers))
    return false;

  const IMAGE_DATA_DIRECTORY& data_directory =
      nt_headers.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
  if (data_directory.VirtualAddress == 0 || data_directory.Size == 0)
    return false;
  IMAGE_DEBUG_DIRECTORY debug_directory;
  if (data_directory.Size % sizeof(debug_directory) != 0)
    return false;
  for (size_t offset = 0; offset < data_directory.Size;
       offset += sizeof(debug_directory)) {
    if (!CheckedReadMemory(Address() + data_directory.VirtualAddress + offset,
                           sizeof(debug_directory),
                           &debug_directory)) {
      LOG(WARNING) << "could not read data directory";
      return false;
    }

    if (debug_directory.Type != IMAGE_DEBUG_TYPE_CODEVIEW)
      continue;

    if (debug_directory.SizeOfData < sizeof(CodeViewRecordPDB70)) {
      LOG(WARNING) << "CodeView debug entry of unexpected size";
      continue;
    }
    scoped_ptr<char[]> data(new char[debug_directory.SizeOfData]);
    if (!CheckedReadMemory(Address() + debug_directory.AddressOfRawData,
                           debug_directory.SizeOfData,
                           data.get())) {
      LOG(WARNING) << "could not read debug directory";
      return false;
    }

    if (*reinterpret_cast<DWORD*>(data.get()) !=
        CodeViewRecordPDB70::kSignature) {
      // TODO(scottmg): Consider supporting other record types, see
      // https://code.google.com/p/crashpad/issues/detail?id=47.
      LOG(WARNING) << "encountered non-7.0 CodeView debug record";
      continue;
    }

    CodeViewRecordPDB70* codeview =
        reinterpret_cast<CodeViewRecordPDB70*>(data.get());
    *uuid = codeview->uuid;
    *age = codeview->age;
    // This is a NUL-terminated string encoded in the codepage of the system
    // where the binary was linked. We have no idea what that was, so we just
    // assume ASCII.
    *pdbname = std::string(reinterpret_cast<char*>(&codeview->pdb_name[0]));
    return true;
  }

  return false;
}

// TODO(scottmg): This needs to be made cross-bitness supporting.
bool PEImageReader::ReadNtHeaders(WinVMAddress* nt_headers_address,
                                  IMAGE_NT_HEADERS* nt_headers) const {
  IMAGE_DOS_HEADER dos_header;
  if (!CheckedReadMemory(Address(), sizeof(IMAGE_DOS_HEADER), &dos_header)) {
    LOG(WARNING) << "could not read dos header of " << module_name_;
    return false;
  }

  if (dos_header.e_magic != IMAGE_DOS_SIGNATURE) {
    LOG(WARNING) << "invalid e_magic in dos header of " << module_name_;
    return false;
  }

  *nt_headers_address = Address() + dos_header.e_lfanew;
  if (!CheckedReadMemory(
          *nt_headers_address, sizeof(IMAGE_NT_HEADERS), nt_headers)) {
    LOG(WARNING) << "could not read nt headers of " << module_name_;
    return false;
  }

  if (nt_headers->Signature != IMAGE_NT_SIGNATURE) {
    LOG(WARNING) << "invalid signature in nt headers of " << module_name_;
    return false;
  }

  return true;
}

bool PEImageReader::GetSectionByName(const std::string& name,
                                     IMAGE_SECTION_HEADER* section) const {
  if (name.size() > sizeof(section->Name)) {
    LOG(WARNING) << "supplied section name too long " << name;
    return false;
  }

  WinVMAddress nt_headers_address;
  IMAGE_NT_HEADERS nt_headers;
  if (!ReadNtHeaders(&nt_headers_address, &nt_headers))
    return false;

  WinVMAddress first_section_address =
      nt_headers_address + offsetof(IMAGE_NT_HEADERS, OptionalHeader) +
      nt_headers.FileHeader.SizeOfOptionalHeader;
  for (DWORD i = 0; i < nt_headers.FileHeader.NumberOfSections; ++i) {
    WinVMAddress section_address =
        first_section_address + sizeof(IMAGE_SECTION_HEADER) * i;
    if (!CheckedReadMemory(
            section_address, sizeof(IMAGE_SECTION_HEADER), section)) {
      LOG(WARNING) << "could not read section " << i << " of " << module_name_;
      return false;
    }
    if (strncmp(reinterpret_cast<const char*>(section->Name),
                name.c_str(),
                sizeof(section->Name)) == 0) {
      return true;
    }
  }

  return false;
}

bool PEImageReader::CheckedReadMemory(WinVMAddress address,
                                      WinVMSize size,
                                      void* into) const {
  CheckedWinAddressRange read_range(process_reader_->Is64Bit(), address, size);
  if (!read_range.IsValid()) {
    LOG(WARNING) << "invalid read range: " << RangeToString(read_range);
    return false;
  }
  if (!module_range_.ContainsRange(read_range)) {
    LOG(WARNING) << "attempt to read outside of module " << module_name_
                 << " at range: " << RangeToString(read_range);
    return false;
  }
  return process_reader_->ReadMemory(address, size, into);
}

}  // namespace crashpad
