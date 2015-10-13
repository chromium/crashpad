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

#include "snapshot/win/process_snapshot_win.h"

#include <algorithm>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "snapshot/win/memory_snapshot_win.h"
#include "snapshot/win/module_snapshot_win.h"
#include "util/win/registration_protocol_win.h"
#include "util/win/time.h"

namespace crashpad {

ProcessSnapshotWin::ProcessSnapshotWin()
    : ProcessSnapshot(),
      system_(),
      threads_(),
      modules_(),
      exception_(),
      memory_map_(),
      process_reader_(),
      report_id_(),
      client_id_(),
      annotations_simple_map_(),
      snapshot_time_(),
      initialized_() {
}

ProcessSnapshotWin::~ProcessSnapshotWin() {
}

bool ProcessSnapshotWin::Initialize(HANDLE process,
                                    ProcessSuspensionState suspension_state) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  GetTimeOfDay(&snapshot_time_);

  if (!process_reader_.Initialize(process, suspension_state))
    return false;

  system_.Initialize(&process_reader_);

  if (process_reader_.Is64Bit())
    InitializePebData<process_types::internal::Traits64>();
  else
    InitializePebData<process_types::internal::Traits32>();

  InitializeThreads();
  InitializeModules();

  for (const MEMORY_BASIC_INFORMATION64& mbi :
       process_reader_.GetProcessInfo().MemoryInfo()) {
    memory_map_.push_back(new internal::MemoryMapRegionSnapshotWin(mbi));
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

bool ProcessSnapshotWin::InitializeException(
    WinVMAddress exception_information_address) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  DCHECK(!exception_);

  ExceptionInformation exception_information;
  if (!process_reader_.ReadMemory(exception_information_address,
                                  sizeof(exception_information),
                                  &exception_information)) {
    LOG(WARNING) << "ReadMemory ExceptionInformation failed";
    return false;
  }

  exception_.reset(new internal::ExceptionSnapshotWin());
  if (!exception_->Initialize(&process_reader_,
                              exception_information.thread_id,
                              exception_information.exception_pointers)) {
    exception_.reset();
    return false;
  }

  return true;
}

void ProcessSnapshotWin::GetCrashpadOptions(
    CrashpadInfoClientOptions* options) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  CrashpadInfoClientOptions local_options;

  for (internal::ModuleSnapshotWin* module : modules_) {
    CrashpadInfoClientOptions module_options;
    module->GetCrashpadOptions(&module_options);

    if (local_options.crashpad_handler_behavior == TriState::kUnset) {
      local_options.crashpad_handler_behavior =
          module_options.crashpad_handler_behavior;
    }
    if (local_options.system_crash_reporter_forwarding == TriState::kUnset) {
      local_options.system_crash_reporter_forwarding =
          module_options.system_crash_reporter_forwarding;
    }

    // If non-default values have been found for all options, the loop can end
    // early.
    if (local_options.crashpad_handler_behavior != TriState::kUnset &&
        local_options.system_crash_reporter_forwarding != TriState::kUnset) {
      break;
    }
  }

  *options = local_options;
}

pid_t ProcessSnapshotWin::ProcessID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return process_reader_.GetProcessInfo().ProcessID();
}

pid_t ProcessSnapshotWin::ParentProcessID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return process_reader_.GetProcessInfo().ParentProcessID();
}

void ProcessSnapshotWin::SnapshotTime(timeval* snapshot_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *snapshot_time = snapshot_time_;
}

void ProcessSnapshotWin::ProcessStartTime(timeval* start_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  process_reader_.StartTime(start_time);
}

void ProcessSnapshotWin::ProcessCPUTimes(timeval* user_time,
                                         timeval* system_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  process_reader_.CPUTimes(user_time, system_time);
}

void ProcessSnapshotWin::ReportID(UUID* report_id) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *report_id = report_id_;
}

void ProcessSnapshotWin::ClientID(UUID* client_id) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *client_id = client_id_;
}

const std::map<std::string, std::string>&
ProcessSnapshotWin::AnnotationsSimpleMap() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return annotations_simple_map_;
}

const SystemSnapshot* ProcessSnapshotWin::System() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &system_;
}

std::vector<const ThreadSnapshot*> ProcessSnapshotWin::Threads() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  std::vector<const ThreadSnapshot*> threads;
  for (internal::ThreadSnapshotWin* thread : threads_) {
    threads.push_back(thread);
  }
  return threads;
}

std::vector<const ModuleSnapshot*> ProcessSnapshotWin::Modules() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  std::vector<const ModuleSnapshot*> modules;
  for (internal::ModuleSnapshotWin* module : modules_) {
    modules.push_back(module);
  }
  return modules;
}

const ExceptionSnapshot* ProcessSnapshotWin::Exception() const {
  return exception_.get();
}

std::vector<const MemoryMapRegionSnapshot*> ProcessSnapshotWin::MemoryMap()
    const {
  std::vector<const MemoryMapRegionSnapshot*> memory_map;
  for (const auto& item : memory_map_)
    memory_map.push_back(item);
  return memory_map;
}

std::vector<const MemorySnapshot*> ProcessSnapshotWin::ExtraMemory() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  std::vector<const MemorySnapshot*> extra_memory;
  for (const auto& peb_memory : peb_memory_)
    extra_memory.push_back(peb_memory);
  return extra_memory;
}

void ProcessSnapshotWin::InitializeThreads() {
  const std::vector<ProcessReaderWin::Thread>& process_reader_threads =
      process_reader_.Threads();
  for (const ProcessReaderWin::Thread& process_reader_thread :
       process_reader_threads) {
    auto thread = make_scoped_ptr(new internal::ThreadSnapshotWin());
    if (thread->Initialize(&process_reader_, process_reader_thread)) {
      threads_.push_back(thread.release());
    }
  }
}

void ProcessSnapshotWin::InitializeModules() {
  const std::vector<ProcessInfo::Module>& process_reader_modules =
      process_reader_.Modules();
  for (const ProcessInfo::Module& process_reader_module :
       process_reader_modules) {
    auto module = make_scoped_ptr(new internal::ModuleSnapshotWin());
    if (module->Initialize(&process_reader_, process_reader_module)) {
      modules_.push_back(module.release());
    }
  }
}

template <class Traits>
void ProcessSnapshotWin::InitializePebData() {
  WinVMAddress peb_address;
  WinVMSize peb_size;
  process_reader_.GetProcessInfo().Peb(&peb_address, &peb_size);
  AddMemorySnapshot(peb_address, peb_size, &peb_memory_);

  process_types::PEB<Traits> peb_data;
  if (!process_reader_.ReadMemory(peb_address, peb_size, &peb_data)) {
    LOG(ERROR) << "ReadMemory PEB";
    return;
  }

  process_types::PEB_LDR_DATA<Traits> peb_ldr_data;
  AddMemorySnapshot(peb_data.Ldr, sizeof(peb_ldr_data), &peb_memory_);
  if (!process_reader_.ReadMemory(
          peb_data.Ldr, sizeof(peb_ldr_data), &peb_ldr_data)) {
    LOG(ERROR) << "ReadMemory PEB_LDR_DATA";
  } else {
    // Walk the LDR structure to retrieve its pointed-to data.
    AddMemorySnapshotForLdrLIST_ENTRY(
        peb_ldr_data.InLoadOrderModuleList,
        offsetof(process_types::LDR_DATA_TABLE_ENTRY<Traits>, InLoadOrderLinks),
        &peb_memory_);
    AddMemorySnapshotForLdrLIST_ENTRY(
        peb_ldr_data.InMemoryOrderModuleList,
        offsetof(process_types::LDR_DATA_TABLE_ENTRY<Traits>,
                 InMemoryOrderLinks),
        &peb_memory_);
    AddMemorySnapshotForLdrLIST_ENTRY(
        peb_ldr_data.InInitializationOrderModuleList,
        offsetof(process_types::LDR_DATA_TABLE_ENTRY<Traits>,
                 InInitializationOrderLinks),
        &peb_memory_);
  }

  process_types::RTL_USER_PROCESS_PARAMETERS<Traits> process_parameters;
  if (!process_reader_.ReadMemory(peb_data.ProcessParameters,
                                  sizeof(process_parameters),
                                  &process_parameters)) {
    LOG(ERROR) << "ReadMemory RTL_USER_PROCESS_PARAMETERS";
    return;
  }
  AddMemorySnapshot(
      peb_data.ProcessParameters, sizeof(process_parameters), &peb_memory_);

  AddMemorySnapshotForUNICODE_STRING(
      process_parameters.CurrentDirectory.DosPath, &peb_memory_);
  AddMemorySnapshotForUNICODE_STRING(process_parameters.DllPath, &peb_memory_);
  AddMemorySnapshotForUNICODE_STRING(process_parameters.ImagePathName,
                                     &peb_memory_);
  AddMemorySnapshotForUNICODE_STRING(process_parameters.CommandLine,
                                     &peb_memory_);
  AddMemorySnapshotForUNICODE_STRING(process_parameters.WindowTitle,
                                     &peb_memory_);
  AddMemorySnapshotForUNICODE_STRING(process_parameters.DesktopInfo,
                                     &peb_memory_);
  AddMemorySnapshotForUNICODE_STRING(process_parameters.ShellInfo,
                                     &peb_memory_);
  AddMemorySnapshotForUNICODE_STRING(process_parameters.RuntimeData,
                                     &peb_memory_);
  AddMemorySnapshot(
      process_parameters.Environment,
      DetermineSizeOfEnvironmentBlock(process_parameters.Environment),
      &peb_memory_);
}

void ProcessSnapshotWin::AddMemorySnapshot(
    WinVMAddress address,
    WinVMSize size,
    PointerVector<internal::MemorySnapshotWin>* into) {
  if (size == 0)
    return;

  // Ensure that the entire range is readable. TODO(scottmg): Consider
  // generalizing this as part of
  // https://code.google.com/p/crashpad/issues/detail?id=59.
  auto ranges = process_reader_.GetProcessInfo().GetReadableRanges(
      CheckedRange<WinVMAddress, WinVMSize>(address, size));
  if (ranges.size() != 1) {
    LOG(ERROR) << base::StringPrintf(
        "range at 0x%llx, size 0x%llx fully unreadable", address, size);
    return;
  }
  if (ranges[0].base() != address || ranges[0].size() != size) {
    LOG(ERROR) << base::StringPrintf(
        "some of range at 0x%llx, size 0x%llx unreadable", address, size);
    return;
  }

  // If we have already added this exact range, don't add it again. This is
  // useful for the LDR module lists which are a set of doubly-linked lists, all
  // pointing to the same module name strings.
  // TODO(scottmg): A more general version of this, handling overlapping,
  // contained, etc. https://code.google.com/p/crashpad/issues/detail?id=61.
  for (const auto& memory_snapshot : *into) {
    if (memory_snapshot->Address() == address &&
        memory_snapshot->Size() == size) {
      return;
    }
  }

  internal::MemorySnapshotWin* memory_snapshot =
      new internal::MemorySnapshotWin();
  memory_snapshot->Initialize(&process_reader_, address, size);
  into->push_back(memory_snapshot);
}

template <class Traits>
void ProcessSnapshotWin::AddMemorySnapshotForUNICODE_STRING(
    const process_types::UNICODE_STRING<Traits>& us,
    PointerVector<internal::MemorySnapshotWin>* into) {
  AddMemorySnapshot(us.Buffer, us.Length, into);
}

template <class Traits>
void ProcessSnapshotWin::AddMemorySnapshotForLdrLIST_ENTRY(
      const process_types::LIST_ENTRY<Traits>& le, size_t offset_of_member,
      PointerVector<internal::MemorySnapshotWin>* into) {
  // Walk the doubly-linked list of entries, adding the list memory itself, as
  // well as pointed-to strings.
  Traits::Pointer last = le.Blink;
  process_types::LDR_DATA_TABLE_ENTRY<Traits> entry;
  Traits::Pointer cur = le.Flink;
  for (;;) {
    // |cur| is the pointer to LIST_ENTRY embedded in the LDR_DATA_TABLE_ENTRY.
    // So we need to offset back to the beginning of the structure.
    if (!process_reader_.ReadMemory(
            cur - offset_of_member, sizeof(entry), &entry)) {
      return;
    }
    AddMemorySnapshot(cur - offset_of_member, sizeof(entry), into);
    AddMemorySnapshotForUNICODE_STRING(entry.FullDllName, into);
    AddMemorySnapshotForUNICODE_STRING(entry.BaseDllName, into);

    process_types::LIST_ENTRY<Traits>* links =
        reinterpret_cast<process_types::LIST_ENTRY<Traits>*>(
            reinterpret_cast<unsigned char*>(&entry) + offset_of_member);
    cur = links->Flink;
    if (cur == last)
      break;
  }
}

WinVMSize ProcessSnapshotWin::DetermineSizeOfEnvironmentBlock(
    WinVMAddress start_of_environment_block) {
  // http://blogs.msdn.com/b/oldnewthing/archive/2010/02/03/9957320.aspx On
  // newer OSs there's no stated limit, but in practice grabbing 32k characters
  // should be more than enough.
  std::wstring env_block;
  env_block.resize(32768);
  WinVMSize bytes_read = process_reader_.ReadAvailableMemory(
      start_of_environment_block,
      env_block.size() * sizeof(env_block[0]),
      &env_block[0]);
  env_block.resize(
      static_cast<unsigned int>(bytes_read / sizeof(env_block[0])));
  const wchar_t terminator[] = { 0, 0 };
  size_t at = env_block.find(std::wstring(terminator, arraysize(terminator)));
  if (at != std::wstring::npos)
    env_block.resize(at + arraysize(terminator));

  return env_block.size() * sizeof(env_block[0]);
}

}  // namespace crashpad
