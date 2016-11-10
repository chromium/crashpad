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

#include "util/win/process_info.h"

#include <dbghelp.h>
#include <intrin.h>
#include <wchar.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "gtest/gtest.h"
#include "test/errors.h"
#include "test/paths.h"
#include "test/scoped_temp_dir.h"
#include "test/win/child_launcher.h"
#include "util/file/file_io.h"
#include "util/misc/random_string.h"
#include "util/misc/uuid.h"
#include "util/win/command_line.h"
#include "util/win/get_function.h"
#include "util/win/handle.h"
#include "util/win/scoped_handle.h"

namespace crashpad {
namespace test {
namespace {

const wchar_t kNtdllName[] = L"\\ntdll.dll";

bool IsProcessWow64(HANDLE process_handle) {
  static const auto is_wow64_process =
      GET_FUNCTION(L"kernel32.dll", ::IsWow64Process);
  if (!is_wow64_process)
    return false;
  BOOL is_wow64;
  if (!is_wow64_process(process_handle, &is_wow64)) {
    PLOG(ERROR) << "IsWow64Process";
    return false;
  }
  return !!is_wow64;
}

void VerifyAddressInInCodePage(const ProcessInfo& process_info,
                               WinVMAddress code_address) {
  // Make sure the child code address is an code page address with the right
  // information.
  const ProcessInfo::MemoryBasicInformation64Vector& memory_info =
      process_info.MemoryInfo();
  bool found_region = false;
  for (const auto& mi : memory_info) {
    if (mi.BaseAddress <= code_address &&
        mi.BaseAddress + mi.RegionSize > code_address) {
      EXPECT_EQ(MEM_COMMIT, mi.State);
      EXPECT_EQ(PAGE_EXECUTE_READ, mi.Protect);
      EXPECT_EQ(MEM_IMAGE, mi.Type);
      EXPECT_FALSE(found_region);
      found_region = true;
    }
  }
  EXPECT_TRUE(found_region);
}

TEST(ProcessInfo, Self) {
  ProcessInfo process_info;
  ASSERT_TRUE(process_info.Initialize(GetCurrentProcess()));
  EXPECT_EQ(GetCurrentProcessId(), process_info.ProcessID());
  EXPECT_GT(process_info.ParentProcessID(), 0u);

#if defined(ARCH_CPU_64_BITS)
  EXPECT_TRUE(process_info.Is64Bit());
  EXPECT_FALSE(process_info.IsWow64());
#else
  EXPECT_FALSE(process_info.Is64Bit());
  if (IsProcessWow64(GetCurrentProcess()))
    EXPECT_TRUE(process_info.IsWow64());
  else
    EXPECT_FALSE(process_info.IsWow64());
#endif

  std::wstring command_line;
  EXPECT_TRUE(process_info.CommandLine(&command_line));
  EXPECT_EQ(std::wstring(GetCommandLine()), command_line);

  std::vector<ProcessInfo::Module> modules;
  EXPECT_TRUE(process_info.Modules(&modules));
  ASSERT_GE(modules.size(), 2u);
  const wchar_t kSelfName[] = L"\\crashpad_util_test.exe";
  ASSERT_GE(modules[0].name.size(), wcslen(kSelfName));
  EXPECT_EQ(kSelfName,
            modules[0].name.substr(modules[0].name.size() - wcslen(kSelfName)));
  ASSERT_GE(modules[1].name.size(), wcslen(kNtdllName));
  EXPECT_EQ(
      kNtdllName,
      modules[1].name.substr(modules[1].name.size() - wcslen(kNtdllName)));

  EXPECT_EQ(reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr)),
            modules[0].dll_base);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(GetModuleHandle(L"ntdll.dll")),
            modules[1].dll_base);

  EXPECT_GT(modules[0].size, 0);
  EXPECT_GT(modules[1].size, 0);

  EXPECT_EQ(GetTimestampForLoadedLibrary(GetModuleHandle(nullptr)),
            modules[0].timestamp);
  // System modules are forced to particular stamps and the file header values
  // don't match the on-disk times. Just make sure we got some data here.
  EXPECT_GT(modules[1].timestamp, 0);

  // Find something we know is a code address and confirm expected memory
  // information settings.
  VerifyAddressInInCodePage(process_info,
                            reinterpret_cast<WinVMAddress>(_ReturnAddress()));
}

void TestOtherProcess(const base::string16& directory_modification) {
  ProcessInfo process_info;

  UUID done_uuid;
  done_uuid.InitializeWithNew();

  ScopedKernelHANDLE done(
      CreateEvent(nullptr, true, false, done_uuid.ToString16().c_str()));
  ASSERT_TRUE(done.get());

  base::FilePath test_executable = Paths::Executable();

  std::wstring child_test_executable =
      test_executable.DirName()
          .Append(directory_modification)
          .Append(test_executable.BaseName().RemoveFinalExtension().value() +
                  L"_process_info_test_child.exe")
          .value();

  std::wstring args;
  AppendCommandLineArgument(done_uuid.ToString16(), &args);

  ChildLauncher child(child_test_executable, args);
  child.Start();

  // The child sends us a code address we can look up in the memory map.
  WinVMAddress code_address;
  CheckedReadFile(
      child.stdout_read_handle(), &code_address, sizeof(code_address));

  ASSERT_TRUE(process_info.Initialize(child.process_handle()));

  // Tell the test it's OK to shut down now that we've read our data.
  EXPECT_TRUE(SetEvent(done.get()));

  std::vector<ProcessInfo::Module> modules;
  EXPECT_TRUE(process_info.Modules(&modules));
  ASSERT_GE(modules.size(), 3u);
  std::wstring child_name = L"\\crashpad_util_test_process_info_test_child.exe";
  ASSERT_GE(modules[0].name.size(), child_name.size());
  EXPECT_EQ(child_name,
            modules[0].name.substr(modules[0].name.size() - child_name.size()));
  ASSERT_GE(modules[1].name.size(), wcslen(kNtdllName));
  EXPECT_EQ(
      kNtdllName,
      modules[1].name.substr(modules[1].name.size() - wcslen(kNtdllName)));
  // lz32.dll is an uncommonly-used-but-always-available module that the test
  // binary manually loads.
  const wchar_t kLz32dllName[] = L"\\lz32.dll";
  ASSERT_GE(modules.back().name.size(), wcslen(kLz32dllName));
  EXPECT_EQ(kLz32dllName,
            modules.back().name.substr(modules.back().name.size() -
                                       wcslen(kLz32dllName)));

  VerifyAddressInInCodePage(process_info, code_address);
}

TEST(ProcessInfo, OtherProcess) {
  TestOtherProcess(FILE_PATH_LITERAL("."));
}

#if defined(ARCH_CPU_64_BITS)
TEST(ProcessInfo, OtherProcessWOW64) {
#ifndef NDEBUG
  TestOtherProcess(FILE_PATH_LITERAL("..\\..\\out\\Debug"));
#else
  TestOtherProcess(FILE_PATH_LITERAL("..\\..\\out\\Release"));
#endif
}
#endif  // ARCH_CPU_64_BITS

TEST(ProcessInfo, AccessibleRangesNone) {
  ProcessInfo::MemoryBasicInformation64Vector memory_info;
  MEMORY_BASIC_INFORMATION64 mbi = {0};

  mbi.BaseAddress = 0;
  mbi.RegionSize = 10;
  mbi.State = MEM_FREE;
  memory_info.push_back(mbi);

  std::vector<CheckedRange<WinVMAddress, WinVMSize>> result =
      GetReadableRangesOfMemoryMap(CheckedRange<WinVMAddress, WinVMSize>(2, 4),
                                   memory_info);

  EXPECT_TRUE(result.empty());
}

TEST(ProcessInfo, AccessibleRangesOneInside) {
  ProcessInfo::MemoryBasicInformation64Vector memory_info;
  MEMORY_BASIC_INFORMATION64 mbi = {0};

  mbi.BaseAddress = 0;
  mbi.RegionSize = 10;
  mbi.State = MEM_COMMIT;
  memory_info.push_back(mbi);

  std::vector<CheckedRange<WinVMAddress, WinVMSize>> result =
      GetReadableRangesOfMemoryMap(CheckedRange<WinVMAddress, WinVMSize>(2, 4),
                                   memory_info);

  ASSERT_EQ(1u, result.size());
  EXPECT_EQ(2, result[0].base());
  EXPECT_EQ(4, result[0].size());
}

TEST(ProcessInfo, AccessibleRangesOneTruncatedSize) {
  ProcessInfo::MemoryBasicInformation64Vector memory_info;
  MEMORY_BASIC_INFORMATION64 mbi = {0};

  mbi.BaseAddress = 0;
  mbi.RegionSize = 10;
  mbi.State = MEM_COMMIT;
  memory_info.push_back(mbi);

  mbi.BaseAddress = 10;
  mbi.RegionSize = 20;
  mbi.State = MEM_FREE;
  memory_info.push_back(mbi);

  std::vector<CheckedRange<WinVMAddress, WinVMSize>> result =
      GetReadableRangesOfMemoryMap(CheckedRange<WinVMAddress, WinVMSize>(5, 10),
                                   memory_info);

  ASSERT_EQ(1u, result.size());
  EXPECT_EQ(5, result[0].base());
  EXPECT_EQ(5, result[0].size());
}

TEST(ProcessInfo, AccessibleRangesOneMovedStart) {
  ProcessInfo::MemoryBasicInformation64Vector memory_info;
  MEMORY_BASIC_INFORMATION64 mbi = {0};

  mbi.BaseAddress = 0;
  mbi.RegionSize = 10;
  mbi.State = MEM_FREE;
  memory_info.push_back(mbi);

  mbi.BaseAddress = 10;
  mbi.RegionSize = 20;
  mbi.State = MEM_COMMIT;
  memory_info.push_back(mbi);

  std::vector<CheckedRange<WinVMAddress, WinVMSize>> result =
      GetReadableRangesOfMemoryMap(CheckedRange<WinVMAddress, WinVMSize>(5, 10),
                                   memory_info);

  ASSERT_EQ(1u, result.size());
  EXPECT_EQ(10, result[0].base());
  EXPECT_EQ(5, result[0].size());
}

TEST(ProcessInfo, ReserveIsInaccessible) {
  ProcessInfo::MemoryBasicInformation64Vector memory_info;
  MEMORY_BASIC_INFORMATION64 mbi = {0};

  mbi.BaseAddress = 0;
  mbi.RegionSize = 10;
  mbi.State = MEM_RESERVE;
  memory_info.push_back(mbi);

  mbi.BaseAddress = 10;
  mbi.RegionSize = 20;
  mbi.State = MEM_COMMIT;
  memory_info.push_back(mbi);

  std::vector<CheckedRange<WinVMAddress, WinVMSize>> result =
      GetReadableRangesOfMemoryMap(CheckedRange<WinVMAddress, WinVMSize>(5, 10),
                                   memory_info);

  ASSERT_EQ(1u, result.size());
  EXPECT_EQ(10, result[0].base());
  EXPECT_EQ(5, result[0].size());
}

TEST(ProcessInfo, PageGuardIsInaccessible) {
  ProcessInfo::MemoryBasicInformation64Vector memory_info;
  MEMORY_BASIC_INFORMATION64 mbi = {0};

  mbi.BaseAddress = 0;
  mbi.RegionSize = 10;
  mbi.State = MEM_COMMIT;
  mbi.Protect = PAGE_GUARD;
  memory_info.push_back(mbi);

  mbi.BaseAddress = 10;
  mbi.RegionSize = 20;
  mbi.State = MEM_COMMIT;
  mbi.Protect = 0;
  memory_info.push_back(mbi);

  std::vector<CheckedRange<WinVMAddress, WinVMSize>> result =
      GetReadableRangesOfMemoryMap(CheckedRange<WinVMAddress, WinVMSize>(5, 10),
                                   memory_info);

  ASSERT_EQ(1u, result.size());
  EXPECT_EQ(10, result[0].base());
  EXPECT_EQ(5, result[0].size());
}

TEST(ProcessInfo, PageNoAccessIsInaccessible) {
  ProcessInfo::MemoryBasicInformation64Vector memory_info;
  MEMORY_BASIC_INFORMATION64 mbi = {0};

  mbi.BaseAddress = 0;
  mbi.RegionSize = 10;
  mbi.State = MEM_COMMIT;
  mbi.Protect = PAGE_NOACCESS;
  memory_info.push_back(mbi);

  mbi.BaseAddress = 10;
  mbi.RegionSize = 20;
  mbi.State = MEM_COMMIT;
  mbi.Protect = 0;
  memory_info.push_back(mbi);

  std::vector<CheckedRange<WinVMAddress, WinVMSize>> result =
      GetReadableRangesOfMemoryMap(CheckedRange<WinVMAddress, WinVMSize>(5, 10),
                                   memory_info);

  ASSERT_EQ(1u, result.size());
  EXPECT_EQ(10, result[0].base());
  EXPECT_EQ(5, result[0].size());
}

TEST(ProcessInfo, AccessibleRangesCoalesced) {
  ProcessInfo::MemoryBasicInformation64Vector memory_info;
  MEMORY_BASIC_INFORMATION64 mbi = {0};

  mbi.BaseAddress = 0;
  mbi.RegionSize = 10;
  mbi.State = MEM_FREE;
  memory_info.push_back(mbi);

  mbi.BaseAddress = 10;
  mbi.RegionSize = 2;
  mbi.State = MEM_COMMIT;
  memory_info.push_back(mbi);

  mbi.BaseAddress = 12;
  mbi.RegionSize = 5;
  mbi.State = MEM_COMMIT;
  memory_info.push_back(mbi);

  std::vector<CheckedRange<WinVMAddress, WinVMSize>> result =
      GetReadableRangesOfMemoryMap(CheckedRange<WinVMAddress, WinVMSize>(11, 4),
                                   memory_info);

  ASSERT_EQ(1u, result.size());
  EXPECT_EQ(11, result[0].base());
  EXPECT_EQ(4, result[0].size());
}

TEST(ProcessInfo, AccessibleRangesMiddleUnavailable) {
  ProcessInfo::MemoryBasicInformation64Vector memory_info;
  MEMORY_BASIC_INFORMATION64 mbi = {0};

  mbi.BaseAddress = 0;
  mbi.RegionSize = 10;
  mbi.State = MEM_COMMIT;
  memory_info.push_back(mbi);

  mbi.BaseAddress = 10;
  mbi.RegionSize = 5;
  mbi.State = MEM_FREE;
  memory_info.push_back(mbi);

  mbi.BaseAddress = 15;
  mbi.RegionSize = 100;
  mbi.State = MEM_COMMIT;
  memory_info.push_back(mbi);

  std::vector<CheckedRange<WinVMAddress, WinVMSize>> result =
      GetReadableRangesOfMemoryMap(CheckedRange<WinVMAddress, WinVMSize>(5, 45),
                                   memory_info);

  ASSERT_EQ(2u, result.size());
  EXPECT_EQ(5, result[0].base());
  EXPECT_EQ(5, result[0].size());
  EXPECT_EQ(15, result[1].base());
  EXPECT_EQ(35, result[1].size());
}

TEST(ProcessInfo, RequestedBeforeMap) {
  ProcessInfo::MemoryBasicInformation64Vector memory_info;
  MEMORY_BASIC_INFORMATION64 mbi = {0};

  mbi.BaseAddress = 10;
  mbi.RegionSize = 10;
  mbi.State = MEM_COMMIT;
  memory_info.push_back(mbi);

  std::vector<CheckedRange<WinVMAddress, WinVMSize>> result =
      GetReadableRangesOfMemoryMap(CheckedRange<WinVMAddress, WinVMSize>(5, 10),
                                   memory_info);

  ASSERT_EQ(1u, result.size());
  EXPECT_EQ(10, result[0].base());
  EXPECT_EQ(5, result[0].size());
}

TEST(ProcessInfo, RequestedAfterMap) {
  ProcessInfo::MemoryBasicInformation64Vector memory_info;
  MEMORY_BASIC_INFORMATION64 mbi = {0};

  mbi.BaseAddress = 10;
  mbi.RegionSize = 10;
  mbi.State = MEM_COMMIT;
  memory_info.push_back(mbi);

  std::vector<CheckedRange<WinVMAddress, WinVMSize>> result =
      GetReadableRangesOfMemoryMap(
          CheckedRange<WinVMAddress, WinVMSize>(15, 100), memory_info);

  ASSERT_EQ(1u, result.size());
  EXPECT_EQ(15, result[0].base());
  EXPECT_EQ(5, result[0].size());
}

TEST(ProcessInfo, ReadableRanges) {
  SYSTEM_INFO system_info;
  GetSystemInfo(&system_info);

  const size_t kBlockSize = system_info.dwPageSize;

  // Allocate 6 pages, and then commit the second, fourth, and fifth, and mark
  // two as committed, but PAGE_NOACCESS, so we have a setup like this:
  // 0       1       2       3       4       5
  // +-----------------------------------------------+
  // | ????? |       | xxxxx |       |       | ????? |
  // +-----------------------------------------------+
  void* reserve_region =
      VirtualAlloc(nullptr, kBlockSize * 6, MEM_RESERVE, PAGE_READWRITE);
  ASSERT_TRUE(reserve_region);
  uintptr_t reserved_as_int = reinterpret_cast<uintptr_t>(reserve_region);
  void* readable1 =
      VirtualAlloc(reinterpret_cast<void*>(reserved_as_int + kBlockSize),
                   kBlockSize,
                   MEM_COMMIT,
                   PAGE_READWRITE);
  ASSERT_TRUE(readable1);
  void* readable2 =
      VirtualAlloc(reinterpret_cast<void*>(reserved_as_int + (kBlockSize * 3)),
                   kBlockSize * 2,
                   MEM_COMMIT,
                   PAGE_READWRITE);
  ASSERT_TRUE(readable2);

  void* no_access =
      VirtualAlloc(reinterpret_cast<void*>(reserved_as_int + (kBlockSize * 2)),
                   kBlockSize,
                   MEM_COMMIT,
                   PAGE_NOACCESS);
  ASSERT_TRUE(no_access);

  HANDLE current_process = GetCurrentProcess();
  ProcessInfo info;
  info.Initialize(current_process);
  auto ranges = info.GetReadableRanges(
      CheckedRange<WinVMAddress, WinVMSize>(reserved_as_int, kBlockSize * 6));

  ASSERT_EQ(2u, ranges.size());
  EXPECT_EQ(reserved_as_int + kBlockSize, ranges[0].base());
  EXPECT_EQ(kBlockSize, ranges[0].size());
  EXPECT_EQ(reserved_as_int + (kBlockSize * 3), ranges[1].base());
  EXPECT_EQ(kBlockSize * 2, ranges[1].size());

  // Also make sure what we think we can read corresponds with what we can
  // actually read.
  std::unique_ptr<unsigned char[]> into(new unsigned char[kBlockSize * 6]);
  SIZE_T bytes_read;

  EXPECT_TRUE(ReadProcessMemory(
      current_process, readable1, into.get(), kBlockSize, &bytes_read));
  EXPECT_EQ(kBlockSize, bytes_read);

  EXPECT_TRUE(ReadProcessMemory(
      current_process, readable2, into.get(), kBlockSize * 2, &bytes_read));
  EXPECT_EQ(kBlockSize * 2, bytes_read);

  EXPECT_FALSE(ReadProcessMemory(
      current_process, no_access, into.get(), kBlockSize, &bytes_read));
  EXPECT_FALSE(ReadProcessMemory(
      current_process, reserve_region, into.get(), kBlockSize, &bytes_read));
  EXPECT_FALSE(ReadProcessMemory(current_process,
                                 reserve_region,
                                 into.get(),
                                 kBlockSize * 6,
                                 &bytes_read));
}

struct ScopedRegistryKeyCloseTraits {
  static HKEY InvalidValue() {
    return nullptr;
  }
  static void Free(HKEY key) {
    RegCloseKey(key);
  }
};

using ScopedRegistryKey =
    base::ScopedGeneric<HKEY, ScopedRegistryKeyCloseTraits>;

TEST(ProcessInfo, Handles) {
  ScopedTempDir temp_dir;

  ScopedFileHandle file(LoggingOpenFileForWrite(
      temp_dir.path().Append(FILE_PATH_LITERAL("test_file")),
      FileWriteMode::kTruncateOrCreate,
      FilePermissions::kWorldReadable));
  ASSERT_TRUE(file.is_valid());

  SECURITY_ATTRIBUTES security_attributes = {0};
  security_attributes.nLength = sizeof(security_attributes);
  security_attributes.bInheritHandle = true;
  ScopedFileHandle inherited_file(CreateFile(
      temp_dir.path().Append(FILE_PATH_LITERAL("inheritable")).value().c_str(),
      GENERIC_WRITE,
      0,
      &security_attributes,
      CREATE_NEW,
      FILE_ATTRIBUTE_NORMAL,
      nullptr));
  ASSERT_TRUE(inherited_file.is_valid());

  HKEY key;
  ASSERT_EQ(ERROR_SUCCESS,
            RegOpenKeyEx(
                HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft", 0, KEY_READ, &key));
  ScopedRegistryKey scoped_key(key);
  ASSERT_TRUE(scoped_key.is_valid());

  std::wstring mapping_name =
      base::UTF8ToUTF16(base::StringPrintf("Local\\test_mapping_%d_%s",
                                           GetCurrentProcessId(),
                                           RandomString().c_str()));
  ScopedKernelHANDLE mapping(CreateFileMapping(INVALID_HANDLE_VALUE,
                                               nullptr,
                                               PAGE_READWRITE,
                                               0,
                                               1024,
                                               mapping_name.c_str()));
  ASSERT_TRUE(mapping.is_valid()) << ErrorMessage("CreateFileMapping");

  ProcessInfo info;
  info.Initialize(GetCurrentProcess());
  bool found_file_handle = false;
  bool found_inherited_file_handle = false;
  bool found_key_handle = false;
  bool found_mapping_handle = false;
  for (auto handle : info.Handles()) {
    if (handle.handle == HandleToInt(file.get())) {
      EXPECT_FALSE(found_file_handle);
      found_file_handle = true;
      EXPECT_EQ(L"File", handle.type_name);
      EXPECT_EQ(1, handle.handle_count);
      EXPECT_NE(0u, handle.pointer_count);
      EXPECT_EQ(STANDARD_RIGHTS_READ | STANDARD_RIGHTS_WRITE | SYNCHRONIZE,
                handle.granted_access & STANDARD_RIGHTS_ALL);
      EXPECT_EQ(0, handle.attributes);
    }
    if (handle.handle == HandleToInt(inherited_file.get())) {
      EXPECT_FALSE(found_inherited_file_handle);
      found_inherited_file_handle = true;
      EXPECT_EQ(L"File", handle.type_name);
      EXPECT_EQ(1, handle.handle_count);
      EXPECT_NE(0u, handle.pointer_count);
      EXPECT_EQ(STANDARD_RIGHTS_READ | STANDARD_RIGHTS_WRITE | SYNCHRONIZE,
                handle.granted_access & STANDARD_RIGHTS_ALL);

      // OBJ_INHERIT from ntdef.h, but including that conflicts with other
      // headers.
      const int kObjInherit = 0x2;
      EXPECT_EQ(kObjInherit, handle.attributes);
    }
    if (handle.handle == HandleToInt(scoped_key.get())) {
      EXPECT_FALSE(found_key_handle);
      found_key_handle = true;
      EXPECT_EQ(L"Key", handle.type_name);
      EXPECT_EQ(1, handle.handle_count);
      EXPECT_NE(0u, handle.pointer_count);
      EXPECT_EQ(STANDARD_RIGHTS_READ,
                handle.granted_access & STANDARD_RIGHTS_ALL);
      EXPECT_EQ(0, handle.attributes);
    }
    if (handle.handle == HandleToInt(mapping.get())) {
      EXPECT_FALSE(found_mapping_handle);
      found_mapping_handle = true;
      EXPECT_EQ(L"Section", handle.type_name);
      EXPECT_EQ(1, handle.handle_count);
      EXPECT_NE(0u, handle.pointer_count);
      EXPECT_EQ(DELETE | READ_CONTROL | WRITE_DAC | WRITE_OWNER |
                    STANDARD_RIGHTS_READ | STANDARD_RIGHTS_WRITE,
                handle.granted_access & STANDARD_RIGHTS_ALL);
      EXPECT_EQ(0, handle.attributes);
    }
  }
  EXPECT_TRUE(found_file_handle);
  EXPECT_TRUE(found_inherited_file_handle);
  EXPECT_TRUE(found_key_handle);
  EXPECT_TRUE(found_mapping_handle);
}

TEST(ProcessInfo, OutOfRangeCheck) {
  const size_t kAllocationSize = 12345;
  std::unique_ptr<char[]> safe_memory(new char[kAllocationSize]);

  ProcessInfo info;
  info.Initialize(GetCurrentProcess());

  EXPECT_TRUE(
      info.LoggingRangeIsFullyReadable(CheckedRange<WinVMAddress, WinVMSize>(
          reinterpret_cast<WinVMAddress>(safe_memory.get()), kAllocationSize)));
  EXPECT_FALSE(info.LoggingRangeIsFullyReadable(
      CheckedRange<WinVMAddress, WinVMSize>(0, 1024)));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
