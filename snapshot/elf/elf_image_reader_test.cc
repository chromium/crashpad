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
#include "test/multiprocess.h"
#include "util/file/file_io.h"
#include "util/linux/auxiliary_vector.h"
#include "util/linux/memory_map.h"
#include "util/misc/address_types.h"
#include "util/misc/from_pointer_cast.h"
#include "util/process/process_memory_linux.h"

extern "C" {
__attribute__((visibility("default"))) void
ElfImageReaderTestExportedSymbol(){};
}  // extern "C"

namespace crashpad {
namespace test {
namespace {

#if defined(ARCH_CPU_64_BITS)
using NoteHeader = Elf64_Nhdr;
#else
using NoteHeader = Elf32_Nhdr;
#endif  // ARCH_CPU_64_BITS

#define DECLARE_NOTE(owner, type, data, varname)                            \
  __attribute__((section(".note"))) struct {                                \
    NoteHeader header = {sizeof(owner), sizeof(data), type};                \
    __attribute__((aligned(                                                 \
        sizeof(NoteHeader::n_namesz)))) char name[sizeof(owner)] = {owner}; \
                                                                            \
    __attribute__((aligned(                                                 \
        sizeof(NoteHeader::n_namesz)))) char desc[sizeof(data)] = {data};   \
  } varname;

DECLARE_NOTE("crashpad", 1, "descriptor_data", kTestNote);

void LocateExecutable(pid_t pid, bool is_64_bit, VMAddress* elf_address) {
  AuxiliaryVector aux;
  ASSERT_TRUE(aux.Initialize(pid, is_64_bit));

  VMAddress phdrs;
  ASSERT_TRUE(aux.GetValue(AT_PHDR, &phdrs));

  MemoryMap memory_map;
  ASSERT_TRUE(memory_map.Initialize(pid));
  const MemoryMap::Mapping* phdr_mapping = memory_map.FindMapping(phdrs);
  ASSERT_TRUE(phdr_mapping);
  const MemoryMap::Mapping* exe_mapping =
      memory_map.FindFileMmapStart(*phdr_mapping);
  ASSERT_TRUE(exe_mapping);
  *elf_address = exe_mapping->range.Base();
}

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

void ReadThisExecutableInTarget(pid_t pid) {
#if defined(ARCH_CPU_64_BITS)
  constexpr bool am_64_bit = true;
#else
  constexpr bool am_64_bit = false;
#endif  // ARCH_CPU_64_BITS

  VMAddress elf_address;
  LocateExecutable(pid, am_64_bit, &elf_address);

  ProcessMemoryLinux memory;
  ASSERT_TRUE(memory.Initialize(pid));
  ProcessMemoryRange range;
  ASSERT_TRUE(range.Initialize(&memory, am_64_bit));

  ElfImageReader reader;
  ASSERT_TRUE(reader.Initialize(range, elf_address));

  ExpectSymbol(&reader,
               "ElfImageReaderTestExportedSymbol",
               FromPointerCast<VMAddress>(ElfImageReaderTestExportedSymbol));

  ElfImageReader::NoteReader::Result result;
  std::string note_name;
  std::string note_desc;
  uint64_t note_type;

  bool note_found = false;
  ElfImageReader::NoteReader notes = reader.Notes(-1);
  while ((result = notes.NextNote(&note_name, &note_type, &note_desc)) ==
         ElfImageReader::NoteReader::Result::kSuccess) {
    if (note_name == kTestNote.name) {
      EXPECT_EQ(note_type, kTestNote.header.n_type);
      EXPECT_EQ(note_desc, std::string(kTestNote.desc, sizeof(kTestNote.desc)));
      note_found = true;
    }
  }
  EXPECT_EQ(result, ElfImageReader::NoteReader::Result::kNoMoreNotes);
  EXPECT_TRUE(note_found);

  notes = reader.Notes(0);
  EXPECT_EQ(notes.NextNote(&note_name, &note_type, &note_desc),
            ElfImageReader::NoteReader::Result::kNoMoreNotes);
}

// Assumes that libc is loaded at the same address in this process as in the
// target, which it is for the fork test below.
void ReadLibcInTarget(pid_t pid) {
#if defined(ARCH_CPU_64_BITS)
  constexpr bool am_64_bit = true;
#else
  constexpr bool am_64_bit = false;
#endif  // ARCH_CPU_64_BITS

  Dl_info info;
  ASSERT_TRUE(dladdr(reinterpret_cast<void*>(getpid), &info)) << "dladdr:"
                                                              << dlerror();
  VMAddress elf_address = FromPointerCast<VMAddress>(info.dli_fbase);

  ProcessMemoryLinux memory;
  ASSERT_TRUE(memory.Initialize(pid));
  ProcessMemoryRange range;
  ASSERT_TRUE(range.Initialize(&memory, am_64_bit));

  ElfImageReader reader;
  ASSERT_TRUE(reader.Initialize(range, elf_address));

  ExpectSymbol(&reader, "getpid", FromPointerCast<VMAddress>(getpid));
}

class ReadExecutableChildTest : public Multiprocess {
 public:
  ReadExecutableChildTest() : Multiprocess() {}
  ~ReadExecutableChildTest() {}

 private:
  void MultiprocessParent() { ReadThisExecutableInTarget(ChildPID()); }
  void MultiprocessChild() { CheckedReadFileAtEOF(ReadPipeHandle()); }
};

TEST(ElfImageReader, MainExecutableSelf) {
  ReadThisExecutableInTarget(getpid());
}

TEST(ElfImageReader, MainExecutableChild) {
  ReadExecutableChildTest test;
  test.Run();
}

TEST(ElfImageReader, OneModuleSelf) {
  ReadLibcInTarget(getpid());
}

class ReadLibcChildTest : public Multiprocess {
 public:
  ReadLibcChildTest() : Multiprocess() {}
  ~ReadLibcChildTest() {}

 private:
  void MultiprocessParent() { ReadLibcInTarget(ChildPID()); }
  void MultiprocessChild() { CheckedReadFileAtEOF(ReadPipeHandle()); }
};

TEST(ElfImageReader, OneModuleChild) {
  ReadLibcChildTest test;
  test.Run();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
