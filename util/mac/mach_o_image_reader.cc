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

#include "util/mac/mach_o_image_reader.h"

#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <string.h>

#include <limits>
#include <vector>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "util/mac/checked_mach_address_range.h"
#include "util/mac/mach_o_image_segment_reader.h"
#include "util/mac/process_reader.h"

namespace {

const uint32_t kInvalidSegmentIndex = std::numeric_limits<uint32_t>::max();

}  // namespace

namespace crashpad {

MachOImageReader::MachOImageReader()
    : segments_(),
      segment_map_(),
      module_info_(),
      dylinker_name_(),
      uuid_(),
      address_(0),
      size_(0),
      slide_(0),
      source_version_(0),
      symtab_command_(),
      dysymtab_command_(),
      id_dylib_command_(),
      process_reader_(NULL),
      file_type_(0),
      initialized_() {
}

MachOImageReader::~MachOImageReader() {
}

bool MachOImageReader::Initialize(ProcessReader* process_reader,
                                  mach_vm_address_t address,
                                  const std::string& name) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  process_reader_ = process_reader;
  address_ = address;

  module_info_ =
      base::StringPrintf(", module %s, address 0x%llx", name.c_str(), address);

  process_types::mach_header mach_header;
  if (!mach_header.Read(process_reader, address)) {
    LOG(WARNING) << "could not read mach_header" << module_info_;
    return false;
  }

  const bool is_64_bit = process_reader->Is64Bit();
  const uint32_t kExpectedMagic = is_64_bit ? MH_MAGIC_64 : MH_MAGIC;
  if (mach_header.magic != kExpectedMagic) {
    LOG(WARNING) << base::StringPrintf("unexpected mach_header::magic 0x%08x",
                                       mach_header.magic) << module_info_;
    return false;
  }

  file_type_ = mach_header.filetype;

  const uint32_t kExpectedSegmentCommand =
      is_64_bit ? LC_SEGMENT_64 : LC_SEGMENT;
  const uint32_t kUnexpectedSegmentCommand =
      is_64_bit ? LC_SEGMENT : LC_SEGMENT_64;

  const struct {
    // Which method to call when encountering a load command matching |command|.
    bool (MachOImageReader::*function)(mach_vm_address_t, const std::string&);

    // The minimum size that may be allotted to store the load command.
    size_t size;

    // The load command to match.
    uint32_t command;

    // True if the load command must not appear more than one time.
    bool singleton;
  } kLoadCommandReaders[] = {
    {
      &MachOImageReader::ReadSegmentCommand,
      process_types::segment_command::ExpectedSize(process_reader),
      kExpectedSegmentCommand,
      false,
    },
    {
      &MachOImageReader::ReadSymTabCommand,
      process_types::symtab_command::ExpectedSize(process_reader),
      LC_SYMTAB,
      true,
    },
    {
      &MachOImageReader::ReadDySymTabCommand,
      process_types::symtab_command::ExpectedSize(process_reader),
      LC_DYSYMTAB,
      true,
    },
    {
      &MachOImageReader::ReadIdDylibCommand,
      process_types::dylib_command::ExpectedSize(process_reader),
      LC_ID_DYLIB,
      true,
    },
    {
      &MachOImageReader::ReadDylinkerCommand,
      process_types::dylinker_command::ExpectedSize(process_reader),
      LC_LOAD_DYLINKER,
      true,
    },
    {
      &MachOImageReader::ReadDylinkerCommand,
      process_types::dylinker_command::ExpectedSize(process_reader),
      LC_ID_DYLINKER,
      true,
    },
    {
      &MachOImageReader::ReadUUIDCommand,
      process_types::uuid_command::ExpectedSize(process_reader),
      LC_UUID,
      true,
    },
    {
      &MachOImageReader::ReadSourceVersionCommand,
      process_types::source_version_command::ExpectedSize(process_reader),
      LC_SOURCE_VERSION,
      true,
    },

    // When reading a 64-bit process, no 32-bit segment commands should be
    // present, and vice-versa.
    {
      &MachOImageReader::ReadUnexpectedCommand,
      process_types::load_command::ExpectedSize(process_reader),
      kUnexpectedSegmentCommand,
      false,
    },
  };

  // This vector is parallel to the kLoadCommandReaders array, and tracks
  // whether a singleton load command matching the |command| field has been
  // found yet.
  std::vector<uint32_t> singleton_indices(arraysize(kLoadCommandReaders),
                                          kInvalidSegmentIndex);

  size_t offset = mach_header.Size();
  const mach_vm_address_t kLoadCommandAddressLimit =
      address + offset + mach_header.sizeofcmds;

  for (uint32_t load_command_index = 0;
       load_command_index < mach_header.ncmds;
       ++load_command_index) {
    mach_vm_address_t load_command_address = address + offset;
    std::string load_command_info = base::StringPrintf(", load command %u/%u%s",
                                                       load_command_index,
                                                       mach_header.ncmds,
                                                       module_info_.c_str());

    process_types::load_command load_command;

    // Make sure that the basic load command structure doesn’t overflow the
    // space allotted for load commands.
    if (load_command_address + load_command.ExpectedSize(process_reader) >
            kLoadCommandAddressLimit) {
      LOG(WARNING) << base::StringPrintf(
                          "load_command at 0x%llx exceeds sizeofcmds 0x%x",
                          load_command_address,
                          mach_header.sizeofcmds) << load_command_info;
      return false;
    }

    if (!load_command.Read(process_reader, load_command_address)) {
      LOG(WARNING) << "could not read load_command" << load_command_info;
      return false;
    }

    load_command_info = base::StringPrintf(", load command 0x%x %u/%u%s",
                                           load_command.cmd,
                                           load_command_index,
                                           mach_header.ncmds,
                                           module_info_.c_str());

    // Now that the load command’s stated size is known, make sure that it
    // doesn’t overflow the space allotted for load commands.
    if (load_command_address + load_command.cmdsize >
            kLoadCommandAddressLimit) {
      LOG(WARNING)
          << base::StringPrintf(
                 "load_command at 0x%llx cmdsize 0x%x exceeds sizeofcmds 0x%x",
                 load_command_address,
                 load_command.cmdsize,
                 mach_header.sizeofcmds) << load_command_info;
      return false;
    }

    for (size_t reader_index = 0;
         reader_index < arraysize(kLoadCommandReaders);
         ++reader_index) {
      if (load_command.cmd != kLoadCommandReaders[reader_index].command) {
        continue;
      }

      if (load_command.cmdsize < kLoadCommandReaders[reader_index].size) {
        LOG(WARNING)
            << base::StringPrintf(
                   "load command cmdsize 0x%x insufficient for 0x%zx",
                   load_command.cmdsize,
                   kLoadCommandReaders[reader_index].size)
            << load_command_info;
        return false;
      }

      if (kLoadCommandReaders[reader_index].singleton) {
        if (singleton_indices[reader_index] != kInvalidSegmentIndex) {
          LOG(WARNING) << "duplicate load command at "
                       << singleton_indices[reader_index]
                       << load_command_info;
          return false;
        }

        singleton_indices[reader_index] = load_command_index;
      }

      if (!((this)->*(kLoadCommandReaders[reader_index].function))(
              load_command_address, load_command_info)) {
        return false;
      }

      break;
    }

    offset += load_command.cmdsize;
  }

  // This was already checked for the unslid values while the segments were
  // read, but now that the slide is known, check the slid values too. The
  // individual sections don’t need to be checked because they were verified to
  // be contained within their respective segments when the segments were read.
  for (const MachOImageSegmentReader* segment : segments_) {
    mach_vm_address_t slid_segment_address = segment->vmaddr();
    mach_vm_size_t slid_segment_size = segment->vmsize();
    if (segment->SegmentSlides()) {
      slid_segment_address += slide_;
    } else {
      // The non-sliding __PAGEZERO segment extends instead of slides. See
      // MachOImageSegmentReader::SegmentSlides().
      slid_segment_size += slide_;
    }
    CheckedMachAddressRange slid_segment_range(
        process_reader_, slid_segment_address, slid_segment_size);
    if (!slid_segment_range.IsValid()) {
      LOG(WARNING) << base::StringPrintf(
                          "invalid slid segment range 0x%llx + 0x%llx, "
                          "segment ",
                          slid_segment_address,
                          slid_segment_size) << segment->Name() << module_info_;
      return false;
    }
  }

  if (!segment_map_.count(SEG_TEXT)) {
    // The __TEXT segment is required. Even a module with no executable code
    // will have a __TEXT segment encompassing the Mach-O header and load
    // commands. Without a __TEXT segment, |size_| will not have been computed.
    LOG(WARNING) << "no " SEG_TEXT " segment" << module_info_;
    return false;
  }

  if (mach_header.filetype == MH_DYLIB && !id_dylib_command_) {
    // This doesn’t render a module unusable, it’s just weird and worth noting.
    LOG(INFO) << "no LC_ID_DYLIB" << module_info_;
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

const MachOImageSegmentReader* MachOImageReader::GetSegmentByName(
    const std::string& segment_name,
    mach_vm_address_t* address,
    mach_vm_size_t* size) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  const auto& iterator = segment_map_.find(segment_name);
  if (iterator == segment_map_.end()) {
    return NULL;
  }

  const MachOImageSegmentReader* segment = segments_[iterator->second];
  if (address) {
    *address = segment->vmaddr() + (segment->SegmentSlides() ? slide_ : 0);
  }
  if (size) {
    // The non-sliding __PAGEZERO segment extends instead of slides. See
    // MachOImageSegmentReader::SegmentSlides().
    *size = segment->vmsize() + (segment->SegmentSlides() ? 0 : slide_);
  }

  return segment;
}

const process_types::section* MachOImageReader::GetSectionByName(
    const std::string& segment_name,
    const std::string& section_name,
    mach_vm_address_t* address) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  const MachOImageSegmentReader* segment =
      GetSegmentByName(segment_name, NULL, NULL);
  if (!segment) {
    return NULL;
  }

  const process_types::section* section =
      segment->GetSectionByName(section_name);
  if (!section) {
    return NULL;
  }

  if (address) {
    *address = section->addr + (segment->SegmentSlides() ? slide_ : 0);
  }

  return section;
}

const process_types::section* MachOImageReader::GetSectionAtIndex(
    size_t index,
    mach_vm_address_t* address) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  COMPILE_ASSERT(NO_SECT == 0, no_sect_must_be_zero);
  if (index == NO_SECT) {
    LOG(WARNING) << "section index " << index << " out of range";
    return NULL;
  }

  // Switch to a more comfortable 0-based index.
  size_t local_index = index - 1;

  for (const MachOImageSegmentReader* segment : segments_) {
    size_t nsects = segment->nsects();
    if (local_index < nsects) {
      const process_types::section* section =
          segment->GetSectionAtIndex(local_index);

      if (address) {
        *address = section->addr + (segment->SegmentSlides() ? slide_ : 0);
      }

      return section;
    }

    local_index -= nsects;
  }

  LOG(WARNING) << "section index " << index << " out of range";
  return NULL;
}

uint32_t MachOImageReader::DylibVersion() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  DCHECK_EQ(FileType(), static_cast<uint32_t>(MH_DYLIB));

  if (id_dylib_command_) {
    return id_dylib_command_->dylib_current_version;
  }

  // In case this was a weird dylib without an LC_ID_DYLIB command.
  return 0;
}

void MachOImageReader::UUID(crashpad::UUID* uuid) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  memcpy(uuid, &uuid_, sizeof(uuid_));
}

template <typename T>
bool MachOImageReader::ReadLoadCommand(mach_vm_address_t load_command_address,
                                       const std::string& load_command_info,
                                       uint32_t expected_load_command_id,
                                       T* load_command) {
  if (!load_command->Read(process_reader_, load_command_address)) {
    LOG(WARNING) << "could not read load command" << load_command_info;
    return false;
  }

  DCHECK_GE(load_command->cmdsize, load_command->Size());
  DCHECK_EQ(load_command->cmd, expected_load_command_id);
  return true;
}

bool MachOImageReader::ReadSegmentCommand(
    mach_vm_address_t load_command_address,
    const std::string& load_command_info) {
  MachOImageSegmentReader* segment = new MachOImageSegmentReader();
  size_t segment_index = segments_.size();
  segments_.push_back(segment);  // Takes ownership.

  if (!segment->Initialize(
          process_reader_, load_command_address, load_command_info)) {
    segments_.pop_back();
    return false;
  }

  // At this point, the segment itself is considered valid, but if one of the
  // next checks fails, it will render the module invalid. If any of the next
  // checks fail, this method should return false, but it doesn’t need to bother
  // removing the segment from segments_. The segment will be properly released
  // when the image is destroyed, and the image won’t be usable because
  // initialization won’t have completed. Most importantly, leaving the segment
  // in segments_ means that no other structures (such as perhaps segment_map_)
  // become inconsistent or require cleanup.

  const std::string segment_name = segment->Name();
  const auto& iterator = segment_map_.find(segment_name);
  if (iterator != segment_map_.end()) {
    LOG(WARNING) << base::StringPrintf("duplicate %s segment at %zu and %zu",
                                       segment_name.c_str(),
                                       iterator->second,
                                       segment_index) << load_command_info;
    return false;
  }
  segment_map_[segment_name] = segment_index;

  mach_vm_size_t vmsize = segment->vmsize();

  if (segment_name == SEG_TEXT) {
    if (vmsize == 0) {
      LOG(WARNING) << "zero-sized " SEG_TEXT " segment" << load_command_info;
      return false;
    }

    mach_vm_size_t fileoff = segment->fileoff();
    if (fileoff != 0) {
      LOG(WARNING) << base::StringPrintf(
                          SEG_TEXT " segment has unexpected fileoff 0x%llx",
                          fileoff) << load_command_info;
      return false;
    }

    size_ = vmsize;

    // The slide is computed as the difference between the __TEXT segment’s
    // preferred and actual load addresses. This is the same way that dyld
    // computes slide. See 10.9.2 dyld-239.4/src/dyldInitialization.cpp
    // slideOfMainExecutable().
    slide_ = address_ - segment->vmaddr();
  }

  return true;
}

bool MachOImageReader::ReadSymTabCommand(mach_vm_address_t load_command_address,
                                         const std::string& load_command_info) {
  symtab_command_.reset(new process_types::symtab_command());
  return ReadLoadCommand(load_command_address,
                         load_command_info,
                         LC_SYMTAB,
                         symtab_command_.get());
}

bool MachOImageReader::ReadDySymTabCommand(
    mach_vm_address_t load_command_address,
    const std::string& load_command_info) {
  dysymtab_command_.reset(new process_types::dysymtab_command());
  return ReadLoadCommand(load_command_address,
                         load_command_info,
                         LC_DYSYMTAB,
                         dysymtab_command_.get());
}

bool MachOImageReader::ReadIdDylibCommand(
    mach_vm_address_t load_command_address,
    const std::string& load_command_info) {
  if (file_type_ != MH_DYLIB) {
    LOG(WARNING) << base::StringPrintf(
                        "LC_ID_DYLIB inappropriate in non-dylib file type 0x%x",
                        file_type_) << load_command_info;
    return false;
  }

  DCHECK(!id_dylib_command_);
  id_dylib_command_.reset(new process_types::dylib_command());
  return ReadLoadCommand(load_command_address,
                         load_command_info,
                         LC_ID_DYLIB,
                         id_dylib_command_.get());
}

bool MachOImageReader::ReadDylinkerCommand(
    mach_vm_address_t load_command_address,
    const std::string& load_command_info) {
  if (file_type_ != MH_EXECUTE && file_type_ != MH_DYLINKER) {
    LOG(WARNING) << base::StringPrintf(
                        "LC_LOAD_DYLINKER/LC_ID_DYLINKER inappropriate in file "
                        "type 0x%x",
                        file_type_) << load_command_info;
    return false;
  }

  const uint32_t kExpectedCommand =
      file_type_ == MH_DYLINKER ? LC_ID_DYLINKER : LC_LOAD_DYLINKER;
  process_types::dylinker_command dylinker_command;
  if (!ReadLoadCommand(load_command_address,
                       load_command_info,
                       kExpectedCommand,
                       &dylinker_command)) {
    return false;
  }

  if (!process_reader_->Memory()->ReadCStringSizeLimited(
          load_command_address + dylinker_command.name,
          dylinker_command.cmdsize - dylinker_command.name,
          &dylinker_name_)) {
    LOG(WARNING) << "could not read dylinker_command name" << load_command_info;
    return false;
  }

  return true;
}

bool MachOImageReader::ReadUUIDCommand(mach_vm_address_t load_command_address,
                                       const std::string& load_command_info) {
  process_types::uuid_command uuid_command;
  if (!ReadLoadCommand(
          load_command_address, load_command_info, LC_UUID, &uuid_command)) {
    return false;
  }

  uuid_.InitializeFromBytes(uuid_command.uuid);
  return true;
}

bool MachOImageReader::ReadSourceVersionCommand(
    mach_vm_address_t load_command_address,
    const std::string& load_command_info) {
  process_types::source_version_command source_version_command;
  if (!ReadLoadCommand(load_command_address,
                       load_command_info,
                       LC_SOURCE_VERSION,
                       &source_version_command)) {
    return false;
  }

  source_version_ = source_version_command.version;
  return true;
}

bool MachOImageReader::ReadUnexpectedCommand(
    mach_vm_address_t load_command_address,
    const std::string& load_command_info) {
  LOG(WARNING) << "unexpected load command" << load_command_info;
  return false;
}

}  // namespace crashpad
