// Copyright 2020 The Crashpad Authors. All rights reserved.
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

#include "snapshot/ios/process_reader_ios.h"

#include <AvailabilityMacros.h>
#include <mach-o/loader.h>

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "base/strings/stringprintf.h"

namespace crashpad {

ProcessReaderIOS::Thread::Thread()
    : thread_context(),
      float_context(),
      debug_context(),
      id(0),
      stack_region_address(0),
      stack_region_size(0),
      thread_specific_data_address(0),
      port(THREAD_NULL),
      suspend_count(0),
      priority(0) {}

ProcessReaderIOS::Module::Module() : name(), timestamp(0) {}

ProcessReaderIOS::Module::~Module() {}

ProcessReaderIOS::ProcessReaderIOS()
    : process_info_(),
      threads_(),
      modules_(),
      initialized_(),
      is_64_bit_(false),
//      initialized_threads_(false),
      initialized_modules_(false) {}

ProcessReaderIOS::~ProcessReaderIOS() {
}

bool ProcessReaderIOS::Initialize() {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);


  pid_t pid = 0;
  if (!process_info_.InitializeWithPid(pid)) {
    return false;
  }

  is_64_bit_ = process_info_.Is64Bit();

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

void ProcessReaderIOS::StartTime(timeval* start_time) const {
  bool rv = process_info_.StartTime(start_time);
  DCHECK(rv);
}

bool ProcessReaderIOS::CPUTimes(timeval* user_time,
                                timeval* system_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return false;
}

const std::vector<ProcessReaderIOS::Thread>& ProcessReaderIOS::Threads() {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

//  if (!initialized_threads_) {
//    InitializeThreads();
//  }

  return threads_;
}

const std::vector<ProcessReaderIOS::Module>& ProcessReaderIOS::Modules() {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  if (!initialized_modules_) {
    InitializeModules();
  }

  return modules_;
}

void ProcessReaderIOS::InitializeModules() {
  DCHECK(!initialized_modules_);
  DCHECK(modules_.empty());

  initialized_modules_ = true;

  // mach_vm_size_t all_image_info_size;
  // mach_vm_address_t all_image_info_addr =
  //     DyldAllImageInfo(&all_image_info_size);

  // process_types::dyld_all_image_infos all_image_infos;
  // if (!all_image_infos.Read(this, all_image_info_addr)) {
  //   LOG(WARNING) << "could not read dyld_all_image_infos";
  //   return;
  // }

  // if (all_image_infos.version < 1) {
  //   LOG(WARNING) << "unexpected dyld_all_image_infos version "
  //                << all_image_infos.version;
  //   return;
  // }

  // size_t expected_size =
  //     process_types::dyld_all_image_infos::ExpectedSizeForVersion(
  //         this, all_image_infos.version);
  // if (all_image_info_size < expected_size) {
  //   LOG(WARNING) << "small dyld_all_image_infos size " << all_image_info_size
  //                << " < " << expected_size << " for version "
  //                << all_image_infos.version;
  //   return;
  // }

  // // Note that all_image_infos.infoArrayCount may be 0 if a crash occurred while
  // // dyld was loading the executable. This can happen if a required dynamic
  // // library was not found. Similarly, all_image_infos.infoArray may be nullptr
  // // if a crash occurred while dyld was updating it.
  // //
  // // TODO(mark): It may be possible to recover from these situations by looking
  // // through memory mappings for Mach-O images.
  // //
  // // Continue along when this situation is detected, because even without any
  // // images in infoArray, dyldImageLoadAddress may be set, and it may be
  // // possible to recover some information from dyld.
  // if (all_image_infos.infoArrayCount == 0) {
  //   LOG(WARNING) << "all_image_infos.infoArrayCount is zero";
  // } else if (!all_image_infos.infoArray) {
  //   LOG(WARNING) << "all_image_infos.infoArray is nullptr";
  // }

  // std::vector<process_types::dyld_image_info> image_info_vector(
  //     all_image_infos.infoArrayCount);
  // if (!process_types::dyld_image_info::ReadArrayInto(this,
  //                                                    all_image_infos.infoArray,
  //                                                    image_info_vector.size(),
  //                                                    &image_info_vector[0])) {
  //   LOG(WARNING) << "could not read dyld_image_info array";
  //   return;
  // }

  // size_t main_executable_count = 0;
  // bool found_dyld = false;
  // modules_.reserve(image_info_vector.size());
  // for (const process_types::dyld_image_info& image_info : image_info_vector) {
  //   Module module;
  //   module.timestamp = image_info.imageFileModDate;

  //   if (!process_memory_.ReadCString(image_info.imageFilePath, &module.name)) {
  //     LOG(WARNING) << "could not read dyld_image_info::imageFilePath";
  //     // Proceed anyway with an empty module name.
  //   }

  //   std::unique_ptr<MachOImageReader> reader(new MachOImageReader());
  //   if (!reader->Initialize(this, image_info.imageLoadAddress, module.name)) {
  //     reader.reset();
  //   }

  //   module.reader = reader.get();

  //   uint32_t file_type = reader ? reader->FileType() : 0;

  //   module_readers_.push_back(std::move(reader));
  //   modules_.push_back(module);

  //   if (all_image_infos.version >= 2 && all_image_infos.dyldImageLoadAddress &&
  //       image_info.imageLoadAddress == all_image_infos.dyldImageLoadAddress) {
  //     found_dyld = true;
  //     LOG(WARNING) << base::StringPrintf(
  //         "found dylinker (%s) in dyld_all_image_infos::infoArray",
  //         module.name.c_str());

  //     LOG_IF(WARNING, file_type != MH_DYLINKER)
  //         << base::StringPrintf("dylinker (%s) has unexpected Mach-O type %d",
  //                               module.name.c_str(),
  //                               file_type);
  //   }

  //   if (file_type == MH_EXECUTE) {
  //     // On Mac OS X 10.6, the main executable does not normally show up at
  //     // index 0. This is because of how 10.6.8 dyld-132.13/src/dyld.cpp
  //     // notifyGDB(), the function resposible for causing
  //     // dyld_all_image_infos::infoArray to be updated, is called. It is
  //     // registered to be called when all dependents of an image have been
  //     // mapped (dyld_image_state_dependents_mapped), meaning that the main
  //     // executable won’t be added to the list until all of the libraries it
  //     // depends on are, even though dyld begins looking at the main executable
  //     // first. This changed in later versions of dyld, including those present
  //     // in 10.7. 10.9.4 dyld-239.4/src/dyld.cpp updateAllImages() (renamed from
  //     // notifyGDB()) is registered to be called when an image itself has been
  //     // mapped (dyld_image_state_mapped), regardless of the libraries that it
  //     // depends on.
  //     //
  //     // The interface requires that the main executable be first in the list,
  //     // so swap it into the right position.
  //     size_t index = modules_.size() - 1;
  //     if (main_executable_count == 0) {
  //       std::swap(modules_[0], modules_[index]);
  //     } else {
  //       LOG(WARNING) << base::StringPrintf(
  //           "multiple MH_EXECUTE modules (%s, %s)",
  //           modules_[0].name.c_str(),
  //           modules_[index].name.c_str());
  //     }
  //     ++main_executable_count;
  //   }
  // }

  // LOG_IF(WARNING, main_executable_count == 0) << "no MH_EXECUTE modules";

  // // all_image_infos.infoArray doesn’t include an entry for dyld, but dyld is
  // // loaded into the process’ address space as a module. Its load address is
  // // easily known given a sufficiently recent all_image_infos.version, but the
  // // timestamp and pathname are not given as they are for other modules.
  // //
  // // The timestamp is a lost cause, because the kernel doesn’t record the
  // // timestamp of the dynamic linker at the time it’s loaded in the same way
  // // that dyld records the timestamps of other modules when they’re loaded. (The
  // // timestamp for the main executable is also not reported and appears as 0
  // // even when accessed via dyld APIs, because it’s loaded by the kernel, not by
  // // dyld.)
  // //
  // // The name can be determined, but it’s not as simple as hardcoding the
  // // default "/usr/lib/dyld" because an executable could have specified anything
  // // in its LC_LOAD_DYLINKER command.
  // if (!found_dyld && all_image_infos.version >= 2 &&
  //     all_image_infos.dyldImageLoadAddress) {
  //   Module module;
  //   module.timestamp = 0;

  //   // Examine the executable’s LC_LOAD_DYLINKER load command to find the path
  //   // used to load dyld.
  //   if (all_image_infos.infoArrayCount >= 1 && main_executable_count >= 1) {
  //     module.name = modules_[0].reader->DylinkerName();
  //   }
  //   std::string module_name = !module.name.empty() ? module.name : "(dyld)";

  //   std::unique_ptr<MachOImageReader> reader(new MachOImageReader());
  //   if (!reader->Initialize(
  //           this, all_image_infos.dyldImageLoadAddress, module_name)) {
  //     reader.reset();
  //   }

  //   module.reader = reader.get();

  //   uint32_t file_type = reader ? reader->FileType() : 0;

  //   LOG_IF(WARNING, file_type != MH_DYLINKER)
  //       << base::StringPrintf("dylinker (%s) has unexpected Mach-O type %d",
  //                             module.name.c_str(),
  //                             file_type);

  //   if (module.name.empty() && file_type == MH_DYLINKER) {
  //     // Look inside dyld directly to find its preferred path.
  //     module.name = reader->DylinkerName();
  //   }

  //   if (module.name.empty()) {
  //     module.name = "(dyld)";
  //   }

  //   // dyld is loaded in the process even if its path can’t be determined.
  //   module_readers_.push_back(std::move(reader));
  //   modules_.push_back(module);
  // }
}

}  // namespace crashpad
