
#include "util/linux/debug_rendezvous.h"

#include <link.h>
#include <sys/types.h>

#include "gtest/gtest.h"
#include "test/multiprocess.h"
#include "util/linux/address_types.h"
#include "util/linux/auxiliary_vector.h"
#include "util/linux/memory_map.h"
#include "util/linux/process_memory.h"
#include "util/misc/from_pointer_cast.h"

namespace crashpad {
namespace test {
namespace {

void TestAgainstTarget(pid_t pid, bool is_64_bit) {
  // Use ElfImageReader on the main executable which can tell us the debug
  // address. On some platforms, the linker defines the symbol _r_debug in
  // link.h which we can use to get the address, but unfortunately not Android.
  AuxiliaryVector aux;
  ASSERT_TRUE(aux.Initialize(pid, is_64_bit));

  LinuxVMAddress phdrs;
  ASSERT_TRUE(aux.GetValue(AT_PHDR, &phdrs));

  MemoryMap mappings;
  ASSERT_TRUE(mappings.Initialize(pid));

  const MemoryMap::Mapping* exe_mapping = memory_map.FindMapping(phdrs);
  ASSERT_TRUE(exe_mapping);
  LinuxVMAddress elf_address = exe_mapping->range.Base();

  ProcessMemory memory;
  ASSERT_TRUE(memory.Initialize(pid));

  ElfImageReader exe_reader;
  ASSERT_TRUE(exe_reader.Initialize(&memory, elf_address));
  LinuxVMAddress debug_address;
  ASSERT_TRUE(exe_reader.GetDebugAddress(&debug_address));

  // start the actual tests
  DebugRendezvous debug;
  ASSERT_TRUE(debug.Initialize(memory, debug_address, is_64_bit));

  EXPECT_EQ(debug.Executable()->load_bias, exe_reader.GetLoadBias());

  LinuxVMAddress linker_base;
  ASSERT_TRUE(aux.GetValue(AT_BASE, &linker_base));
  EXPECT_EQ(debug.LinkerBase(), linker_base);

  ElfImageReader linker_reader;
  ASSERT_TRUE(linker_reader.Initialize(&memory, debug.LinkerBase()));

  EXPECT_EQ(debug.Linker()->load_bias, linker_reader.GetLoadBias());

  for (const LinkEntry& module : debug.Modules()) {
    const MemoryMap::Mapping* mapping =
        mappings.FindMapping(module.dynamic_section);
    ASSERT_TRUE(mapping);

    const MemoryMap::Mapping* module_mapping =
        mappings.FindMapingWithName(mapping->name);

    ElfImageReader module_reader;
    ASSERT_TRUE(module_reader.Initialize(memory, module_mapping->range.Base()));
    EXPECT_EQ(module.load_bias, module_reader.GetLoadBias());
  }
}

TEST(DebugRendezvous, Self) {
#if defined(ARCH_CPU_64_BITS)
  constexpr bool am_64_bit = true;
#else
  constexpr bool am_64_bit = false;
#endif

  TestAgainstTarget(getpid(), am_64_bit);
}

class ChildTest : public Multiprocess {
 public:
  ChildTest() {}
  ~ChildTest() {}

 private:
  void MultiprocessParent() {
#if defined(ARCH_CPU_64_BITS)
    constexpr bool am_64_bit = true;
#else
    constexpr bool am_64_bit = false;
#endif

    TestAgainstTarget(ChildPID(), am_64_bit);
  }

  void MultiprocessChild() { CheckedReadFileAtEOF(ReadPipeHandle()); }

  DISALLOW_COPY_AND_ASSIGN(ChildTest);
};

TEST(DebugRendezvous, Child) {
  ChildTest test;
  test.Run();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
