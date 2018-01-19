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

#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "gtest/gtest.h"
#include "test/multiprocess_exec.h"
#include "util/file/file_io.h"
#include "util/misc/address_types.h"
#include "util/misc/from_pointer_cast.h"

#if defined(OS_FUCHSIA)
#include <link.h>
#include <zircon/syscalls.h>

#include "util/process/process_memory_fuchsia.h"
#elif defined(OS_LINUX) || defined(OS_ANDROID)
#include "util/linux/auxiliary_vector.h"
#include "util/linux/memory_map.h"
#include "util/process/process_memory_linux.h"
#else
#error Port.
#endif

extern "C" {
__attribute__((visibility("default"))) void
ElfImageReaderTestExportedSymbol(){};
}  // extern "C"

namespace crashpad {
namespace test {
namespace {

#if defined(OS_FUCHSIA)

template <class T> void Read(zx_handle_t process, zx_vaddr_t address, T* into) {
  size_t actual;
  zx_status_t status =
      zx_process_read_memory(process, address, into, sizeof(T), &actual);
  ZX_CHECK(status == ZX_OK, status) << "zx_process_read_memory";
  CHECK(actual == sizeof(T)) << "zx_process_read_memory: short read";
}

void LocateExecutable(ProcessHandle process,
                      bool is_64_bit,
                      VMAddress* elf_address) {
  uintptr_t debug_address;
  zx_status_t status = zx_object_get_property(process,
                                              ZX_PROP_PROCESS_DEBUG_ADDR,
                                              &debug_address,
                                              sizeof(debug_address));
  ZX_CHECK(status == ZX_OK, status)
      << "zx_object_get_property: ZX_PROP_PROCESS_DEBUG_ADDR";

  constexpr auto k_r_debug_map_offset = offsetof(r_debug, r_map);
  uintptr_t map;
  Read(process, debug_address + k_r_debug_map_offset, &map);

  constexpr auto k_link_map_addr_offset = offsetof(link_map, l_addr);
  uintptr_t base;
  Read(process, map + k_link_map_addr_offset, &base);

  *elf_address = base;
  fprintf(stderr, "hopeful elf_address=0x%lx\n", *elf_address);
}

#elif defined(OS_LINUX) || defined(OS_ANDROID)

void LocateExecutable(ProcessHandle process,
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

void ReadThisExecutableInTarget(ProcessHandle process) {
#if defined(ARCH_CPU_64_BITS)
  constexpr bool am_64_bit = true;
#else
  constexpr bool am_64_bit = false;
#endif  // ARCH_CPU_64_BITS

  VMAddress elf_address;
  LocateExecutable(process, am_64_bit, &elf_address);

#if defined(OS_FUCHSIA)
  ProcessMemoryFuchsia memory;
#elif defined(OS_LINUX) || defined(OS_ANDROID)
  ProcessMemoryLinux memory;
#endif

  LOG(ERROR) << "hai0";
  ASSERT_TRUE(memory.Initialize(process));
  LOG(ERROR) << "hai1";
  ProcessMemoryRange range;
  LOG(ERROR) << "hai2";
  ASSERT_TRUE(range.Initialize(&memory, am_64_bit));
  LOG(ERROR) << "hai3";

  ElfImageReader reader;
  LOG(ERROR) << "hai4";
  ASSERT_TRUE(reader.Initialize(range, elf_address));
  LOG(ERROR) << "hai5";

  ExpectSymbol(&reader,
               "ElfImageReaderTestExportedSymbol",
               FromPointerCast<VMAddress>(ElfImageReaderTestExportedSymbol));
  LOG(ERROR) << "hai6";

  ElfImageReader::NoteReader::Result result;
  LOG(ERROR) << "hai7";
  std::string note_name;
  LOG(ERROR) << "hai8";
  std::string note_desc;
  LOG(ERROR) << "hai9";
  ElfImageReader::NoteReader::NoteType note_type;
  LOG(ERROR) << "hai10";

  std::unique_ptr<ElfImageReader::NoteReader> notes = reader.Notes(-1);
  while ((result = notes->NextNote(&note_name, &note_type, &note_desc)) ==
         ElfImageReader::NoteReader::Result::kSuccess) {
  LOG(ERROR) << "hai11";
  }
  EXPECT_EQ(result, ElfImageReader::NoteReader::Result::kNoMoreNotes);
  LOG(ERROR) << "hai12";

  notes = reader.Notes(0);
  LOG(ERROR) << "hai13";
  EXPECT_EQ(notes->NextNote(&note_name, &note_type, &note_desc),
            ElfImageReader::NoteReader::Result::kNoMoreNotes);
  LOG(ERROR) << "hai14";

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

// Assumes that libc is loaded at the same address in this process as in the
// target, which it is for the fork test below.
void ReadLibcInTarget(ProcessHandle process) {
#if defined(ARCH_CPU_64_BITS)
  constexpr bool am_64_bit = true;
#else
  constexpr bool am_64_bit = false;
#endif  // ARCH_CPU_64_BITS

  Dl_info info;
  ASSERT_TRUE(dladdr(reinterpret_cast<void*>(getpid), &info)) << "dladdr:"
                                                              << dlerror();
  VMAddress elf_address = FromPointerCast<VMAddress>(info.dli_fbase);

#if defined(OS_FUCHSIA)
  ProcessMemoryFuchsia memory;
#elif defined(OS_LINUX) || defined(OS_ANDROID)
  ProcessMemoryLinux memory;
#endif
  ASSERT_TRUE(memory.Initialize(process));
  ProcessMemoryRange range;
  ASSERT_TRUE(range.Initialize(&memory, am_64_bit));

  ElfImageReader reader;
  ASSERT_TRUE(reader.Initialize(range, elf_address));

  ExpectSymbol(&reader, "getpid", FromPointerCast<VMAddress>(getpid));
}

#if 0
class ReadExecutableChildTest : public MultiprocessExec {
 public:
  ReadExecutableChildTest() : MultiprocessExec() {}
  ~ReadExecutableChildTest() {}

 private:
  void MultiprocessParent() { ReadThisExecutableInTarget(ChildPID()); }
  void MultiprocessChild() { CheckedReadFileAtEOF(ReadPipeHandle()); }
};
#endif

TEST(ElfImageReader, MainExecutableSelf) {
  ReadThisExecutableInTarget(GetSelfProcessHandle());
}

#if 0
TEST(ElfImageReader, MainExecutableChild) {
  ReadExecutableChildTest test;
  test.Run();
}
#endif

TEST(ElfImageReader, OneModuleSelf) {
  ReadLibcInTarget(GetSelfProcessHandle());
}

#if 0
class ReadLibcChildTest : public MultiprocessExec {
 public:
  ReadLibcChildTest() : MultiprocessExec() {}
  ~ReadLibcChildTest() {}

 private:
  void MultiprocessParent() { ReadLibcInTarget(ChildPID()); }
  void MultiprocessChild() { CheckedReadFileAtEOF(ReadPipeHandle()); }
};

TEST(ElfImageReader, OneModuleChild) {
  ReadLibcChildTest test;
  test.Run();
}
#endif

}  // namespace
}  // namespace test
}  // namespace crashpad
