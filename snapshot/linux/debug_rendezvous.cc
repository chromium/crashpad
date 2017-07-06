// Copyright 2017 The Crashpad Authors. All rights reserved.
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

#include "snapshot/linux/debug_rendezvous.h"

#include "base/logging.h"

namespace crashpad {

namespace {

struct Traits32 {
  using Integer = int32_t;
  using Address = uint32_t;
};

struct Traits64 {
  using Integer = int64_t;
  using Address = uint64_t;
};

template <typename Traits>
struct DebugRendezvousSpecific {
  typename Traits::Integer r_version;
  typename Traits::Address r_map;
  typename Traits::Address r_brk;
  typename Traits::Integer r_state;
  typename Traits::Address r_ldbase;
};

template <typename Traits>
struct LinkEntrySpecific {
  typename Traits::Address l_addr;
  typename Traits::Address l_name;
  typename Traits::Address l_ld;
  typename Traits::Address l_next;
  typename Traits::Address l_prev;
};

template <typename Traits>
bool ReadLinkEntry(const ProcessMemory& memory,
                   LinuxVMAddress address,
                   DebugRendezvous::LinkEntry* entry_out,
                   LinuxVMAddress* next) {
  LinkEntrySpecific<Traits> entry;
  if (!memory.Read(address, sizeof(entry), &entry)) {
    return false;
  }
  std::string name;
  if (!memory.ReadCStringSizeLimited(entry.l_name, 4096, &name)) {
    return false;
  }

  entry_out->load_bias = entry.l_addr;
  entry_out->dynamic_section = entry.l_ld;
  entry_out->name.swap(name);

  *next = entry.l_next;
  return true;
}

}  // namespace

DebugRendezvous::LinkEntry::LinkEntry()
    : name(), load_bias(0), dynamic_section(0) {}

DebugRendezvous::DebugRendezvous() : modules_(), executable_(), linker_() {}

DebugRendezvous::~DebugRendezvous() {}

bool DebugRendezvous::Initialize(const ProcessMemory& memory,
                                 LinuxVMAddress address,
                                 bool is_64_bit) {
  return is_64_bit ? InitializeSpecific<Traits64>(memory, address)
                   : InitializeSpecific<Traits32>(memory, address);
}

template <typename Traits>
bool DebugRendezvous::InitializeSpecific(const ProcessMemory& memory,
                                         LinuxVMAddress address) {
  DebugRendezvousSpecific<Traits> debug;
  if (!memory.Read(address, sizeof(debug), &debug)) {
    return false;
  }
  linker_base_ = debug.r_ldbase;

  LinuxVMAddress link_entry_address = debug.r_map;
  if (!ReadLinkEntry<Traits>(
          memory, link_entry_address, &executable_, &link_entry_address)) {
    return false;
  }

  if (!ReadLinkEntry<Traits>(
          memory, link_entry_address, &linker_, &link_entry_address)) {
    return false;
  }

  while (link_entry_address) {
    LinkEntry entry;
    if (!ReadLinkEntry<Traits>(
            memory, link_entry_address, &entry, &link_entry_address)) {
      return false;
    }
    modules_.push_back(entry);
  }

  return true;
}

}  // namespace crashpad
