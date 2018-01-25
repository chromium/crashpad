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

#include "snapshot/elf/elf_image_reader.h"

#include <dlfcn.h>
#include <unistd.h>

#include "base/logging.h"
#include "build/build_config.h"
#include "gtest/gtest.h"
#include "test/multiprocess_exec.h"
#include "test/process_type.h"
#include "util/file/file_io.h"
#include "util/misc/address_types.h"
#include "util/misc/from_pointer_cast.h"
#include "util/process/process_memory_native.h"

#if defined(OS_FUCHSIA)

#include <link.h>
#include <zircon/syscalls.h>

#include "base/fuchsia/fuchsia_logging.h"

#elif defined(OS_LINUX) || defined(OS_ANDROID)

#include "util/linux/auxiliary_vector.h"
#include "util/linux/memory_map.h"

#else

#error Port.

#endif  // OS_FUCHSIA

extern "C" {
__attribute__((visibility("default"))) void
ElfImageReaderTestExportedSymbol(){};
}  // extern "C"

namespace crashpad {
namespace test {
namespace {


#if defined(OS_FUCHSIA)

void LocateExecutable(ProcessType process,
                      ProcessMemory* memory,
                      bool is_64_bit,
                      VMAddress* elf_address) {
  uintptr_t debug_address;
  zx_status_t status = zx_object_get_property(process,
                                              ZX_PROP_PROCESS_DEBUG_ADDR,
                                              &debug_address,
                                              sizeof(debug_address));
  ASSERT_EQ(status, ZX_OK)
      << "zx_object_get_property: ZX_PROP_PROCESS_DEBUG_ADDR";
  // Can be 0 if requested before the loader has loaded anything.
  EXPECT_NE(debug_address, 0u);

  constexpr auto k_r_debug_map_offset = offsetof(r_debug, r_map);
  uintptr_t map;
  ASSERT_TRUE(
      memory->Read(debug_address + k_r_debug_map_offset, sizeof(map), &map))
      << "read link_map";

  constexpr auto k_link_map_addr_offset = offsetof(link_map, l_addr);
  uintptr_t base;
  ASSERT_TRUE(memory->Read(map + k_link_map_addr_offset, sizeof(base), &base))
      << "read base";

  *elf_address = base;
}

#elif defined(OS_LINUX) || defined(OS_ANDROID)

void LocateExecutable(ProcessType process,
                      ProcessMemory* memory,
                      bool is_64_bit,
                      VMAddress* elf_address) {
  AuxiliaryVector aux;
  ASSERT_TRUE(aux.Initialize(process, is_64_bit));

  VMAddress phdrs;
  ASSERT_TRUE(aux.GetValue(AT_PHDR, &phdrs));

  MemoryMap memory_map;
  ASSERT_TRUE(memory_map.Initialize(process));
  const MemoryMap::Mapping* phdr_mapping = memory_map.FindMapping(phdrs);
  ASSERT_TRUE(phdr_mapping);
  const MemoryMap::Mapping* exe_mapping =
      memory_map.FindFileMmapStart(*phdr_mapping);
  ASSERT_TRUE(exe_mapping);
  *elf_address = exe_mapping->range.Base();
}

#endif  // OS_FUCHSIA

void ExpectSymbol(ElfImageReader* reader,
                  const std::string& symbol_name,
                  VMAddress expected_symbol_address) {
  VMAddress symbol_address;
  VMSize symbol_size;
  ASSERT_TRUE(
      reader->GetDynamicSymbol(symbol_name, &symbol_address, &symbol_size));
  EXPECT_EQ(symbol_address, expected_symbol_address);

  EXPECT_FALSE(
      reader->GetDynamicSymbol("notasymbol", &symbol_address, &symbol_size));
}

void ReadThisExecutableInTarget(ProcessType process,
                                VMAddress exported_symbol_address) {
#if defined(ARCH_CPU_64_BITS)
  constexpr bool am_64_bit = true;
#else
  constexpr bool am_64_bit = false;
#endif  // ARCH_CPU_64_BITS

  ProcessMemoryNative memory;
  ASSERT_TRUE(memory.Initialize(process));
  ProcessMemoryRange range;
  ASSERT_TRUE(range.Initialize(&memory, am_64_bit));

  VMAddress elf_address;
  LocateExecutable(process, &memory, am_64_bit, &elf_address);
  ASSERT_NO_FATAL_FAILURE();

  ElfImageReader reader;
  ASSERT_TRUE(reader.Initialize(range, elf_address));

  ExpectSymbol(
      &reader, "ElfImageReaderTestExportedSymbol", exported_symbol_address);

  ElfImageReader::NoteReader::Result result;
  std::string note_name;
  std::string note_desc;
  ElfImageReader::NoteReader::NoteType note_type;

  std::unique_ptr<ElfImageReader::NoteReader> notes = reader.Notes(-1);
  while ((result = notes->NextNote(&note_name, &note_type, &note_desc)) ==
         ElfImageReader::NoteReader::Result::kSuccess) {
  }
  EXPECT_EQ(result, ElfImageReader::NoteReader::Result::kNoMoreNotes);

  notes = reader.Notes(0);
  EXPECT_EQ(notes->NextNote(&note_name, &note_type, &note_desc),
            ElfImageReader::NoteReader::Result::kNoMoreNotes);

  // Find the note defined in elf_image_reader_test_note.S.
  constexpr char kCrashpadNoteName[] = "Crashpad";
  constexpr ElfImageReader::NoteReader::NoteType kCrashpadNoteType = 1;
  constexpr uint32_t kCrashpadNoteDesc = 42;
  notes = reader.NotesWithNameAndType(kCrashpadNoteName, kCrashpadNoteType, -1);
  ASSERT_EQ(notes->NextNote(&note_name, &note_type, &note_desc),
            ElfImageReader::NoteReader::Result::kSuccess);
  EXPECT_EQ(note_name, kCrashpadNoteName);
  EXPECT_EQ(note_type, kCrashpadNoteType);
  EXPECT_EQ(note_desc.size(), sizeof(kCrashpadNoteDesc));
  EXPECT_EQ(*reinterpret_cast<decltype(kCrashpadNoteDesc)*>(&note_desc[0]),
            kCrashpadNoteDesc);

  EXPECT_EQ(notes->NextNote(&note_name, &note_type, &note_desc),
            ElfImageReader::NoteReader::Result::kNoMoreNotes);
}

void ReadLibcInTarget(ProcessType process,
                      VMAddress elf_address,
                      VMAddress getpid_address) {
#if defined(ARCH_CPU_64_BITS)
  constexpr bool am_64_bit = true;
#else
  constexpr bool am_64_bit = false;
#endif  // ARCH_CPU_64_BITS

  ProcessMemoryNative memory;
  ASSERT_TRUE(memory.Initialize(process));
  ProcessMemoryRange range;
  ASSERT_TRUE(range.Initialize(&memory, am_64_bit));

  ElfImageReader reader;
  ASSERT_TRUE(reader.Initialize(range, elf_address));

  ExpectSymbol(&reader, "getpid", getpid_address);
}

TEST(ElfImageReader, MainExecutableSelf) {
  ReadThisExecutableInTarget(
      GetSelfProcess(),
      FromPointerCast<VMAddress>(ElfImageReaderTestExportedSymbol));
}

CRASHPAD_CHILD_TEST_MAIN(ReadExecutableChild) {
  VMAddress exported_symbol_address =
      FromPointerCast<VMAddress>(ElfImageReaderTestExportedSymbol);
  CheckedWriteFile(StdioFileHandle(StdioStream::kStandardOutput),
                   &exported_symbol_address,
                   sizeof(exported_symbol_address));
  CheckedReadFileAtEOF(StdioFileHandle(StdioStream::kStandardInput));
  return 0;
}

class ReadExecutableChildTest : public MultiprocessExec {
 public:
  ReadExecutableChildTest() : MultiprocessExec() {}

 private:
  void MultiprocessParent() {
    // This read serves two purposes -- on Fuchsia, the loader may have not
    // filled in debug address as soon as the child process handle is valid, so
    // this causes a wait at least until the main() of the child, at which point
    // it will always be valid. Secondarily, the address of the symbol to be
    // looked up needs to be communicated.
    VMAddress exported_symbol_address;
    CheckedReadFileExactly(ReadPipeHandle(),
                           &exported_symbol_address,
                           sizeof(exported_symbol_address));
    ReadThisExecutableInTarget(ChildProcess(), exported_symbol_address);
  }
};

TEST(ElfImageReader, MainExecutableChild) {
  ReadExecutableChildTest test;
  test.SetChildTestMainFunction("ReadExecutableChild");
  test.Run();
}

TEST(ElfImageReader, OneModuleSelf) {
  Dl_info info;
  ASSERT_TRUE(dladdr(reinterpret_cast<void*>(getpid), &info)) << "dladdr:"
                                                              << dlerror();
  VMAddress elf_address = FromPointerCast<VMAddress>(info.dli_fbase);
  ReadLibcInTarget(
      GetSelfProcess(), elf_address, FromPointerCast<VMAddress>(getpid));
}

CRASHPAD_CHILD_TEST_MAIN(ReadLibcChild) {
  // Get the address of libc (by using getpid() as a representative member),
  // and also the address of getpid() itself, and write them to the parent, so
  // it can validate reading this information back out.
  Dl_info info;
  EXPECT_TRUE(dladdr(reinterpret_cast<void*>(getpid), &info))
      << "dladdr:" << dlerror();
  VMAddress elf_address = FromPointerCast<VMAddress>(info.dli_fbase);
  VMAddress getpid_address = FromPointerCast<VMAddress>(getpid);

  CheckedWriteFile(StdioFileHandle(StdioStream::kStandardOutput),
                   &elf_address,
                   sizeof(elf_address));
  CheckedWriteFile(StdioFileHandle(StdioStream::kStandardOutput),
                   &getpid_address,
                   sizeof(getpid_address));
  CheckedReadFileAtEOF(StdioFileHandle(StdioStream::kStandardInput));
  return 0;
}

class ReadLibcChildTest : public MultiprocessExec {
 public:
  ReadLibcChildTest() : MultiprocessExec() {}
  ~ReadLibcChildTest() {}

 private:
  void MultiprocessParent() {
    VMAddress elf_address, getpid_address;
    CheckedReadFileExactly(ReadPipeHandle(), &elf_address, sizeof(elf_address));
    CheckedReadFileExactly(
        ReadPipeHandle(), &getpid_address, sizeof(getpid_address));
    ReadLibcInTarget(ChildProcess(), elf_address, getpid_address);
  }
};

TEST(ElfImageReader, OneModuleChild) {
  ReadLibcChildTest test;
  test.SetChildTestMainFunction("ReadLibcChild");
  test.Run();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
