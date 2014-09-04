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

#include <dlfcn.h>
#include <mach-o/dyld_images.h>
#include <mach-o/getsect.h>
#include <mach-o/loader.h>
#include <stdint.h>

#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "gtest/gtest.h"
#include "util/mac/mach_o_image_segment_reader.h"
#include "util/mac/process_reader.h"
#include "util/mac/process_types.h"
#include "util/misc/uuid.h"
#include "util/test/mac/dyld.h"

namespace {

using namespace crashpad;

// Native types and constants, in cases where the 32-bit and 64-bit versions
// are different.
#if defined(ARCH_CPU_64_BITS)
typedef mach_header_64 MachHeader;
const uint32_t kMachMagic = MH_MAGIC_64;
typedef segment_command_64 SegmentCommand;
const uint32_t kSegmentCommand = LC_SEGMENT_64;
typedef section_64 Section;
#else
typedef mach_header MachHeader;
const uint32_t kMachMagic = MH_MAGIC;
typedef segment_command SegmentCommand;
const uint32_t kSegmentCommand = LC_SEGMENT;
typedef section Section;
#endif

#if defined(ARCH_CPU_X86_64)
const int kCPUType = CPU_TYPE_X86_64;
#elif defined(ARCH_CPU_X86)
const int kCPUType = CPU_TYPE_X86;
#endif

// Verifies that |expect_section| and |actual_section| agree.
void ExpectSection(const Section* expect_section,
                   const process_types::section* actual_section) {
  ASSERT_TRUE(expect_section);
  ASSERT_TRUE(actual_section);

  EXPECT_EQ(
      MachOImageSegmentReader::SectionNameString(expect_section->sectname),
      MachOImageSegmentReader::SectionNameString(actual_section->sectname));
  EXPECT_EQ(
      MachOImageSegmentReader::SegmentNameString(expect_section->segname),
      MachOImageSegmentReader::SegmentNameString(actual_section->segname));
  EXPECT_EQ(expect_section->addr, actual_section->addr);
  EXPECT_EQ(expect_section->size, actual_section->size);
  EXPECT_EQ(expect_section->offset, actual_section->offset);
  EXPECT_EQ(expect_section->align, actual_section->align);
  EXPECT_EQ(expect_section->reloff, actual_section->reloff);
  EXPECT_EQ(expect_section->nreloc, actual_section->nreloc);
  EXPECT_EQ(expect_section->flags, actual_section->flags);
  EXPECT_EQ(expect_section->reserved1, actual_section->reserved1);
  EXPECT_EQ(expect_section->reserved2, actual_section->reserved2);
}

// Verifies that |expect_segment| is a valid Mach-O segment load command for the
// current system by checking its |cmd| field. Then, verifies that the
// information in |actual_segment| matches that in |expect_segment|. The
// |segname|, |vmaddr|, |vmsize|, and |fileoff| fields are examined. Each
// section within the segment is also examined by calling ExpectSection().
// Access to each section via both MachOImageSegmentReader::GetSectionByName()
// and MachOImageReader::GetSectionByName() is verified, expecting that each
// call produces the same section. Segment and section data addresses are
// verified against data obtained by calling getsegmentdata() and
// getsectiondata(). The segment is checked to make sure that it behaves
// correctly when attempting to look up a nonexistent section by name.
// |section_index| is used to track the last-used section index in an image on
// entry, and is reset to the last-used section index on return after the
// sections are processed. This is used to test that
// MachOImageReader::GetSectionAtIndex() returns the correct result.
void ExpectSegmentCommand(const SegmentCommand* expect_segment,
                          const MachHeader* expect_image,
                          const MachOImageSegmentReader* actual_segment,
                          mach_vm_address_t actual_segment_address,
                          mach_vm_size_t actual_segment_size,
                          const MachOImageReader* actual_image,
                          size_t* section_index) {
  ASSERT_TRUE(expect_segment);
  ASSERT_TRUE(actual_segment);

  EXPECT_EQ(kSegmentCommand, expect_segment->cmd);

  std::string segment_name = actual_segment->Name();
  EXPECT_EQ(MachOImageSegmentReader::SegmentNameString(expect_segment->segname),
            segment_name);
  EXPECT_EQ(expect_segment->vmaddr, actual_segment->vmaddr());
  EXPECT_EQ(expect_segment->vmsize, actual_segment->vmsize());
  EXPECT_EQ(expect_segment->fileoff, actual_segment->fileoff());

  if (actual_segment->SegmentSlides()) {
    EXPECT_EQ(actual_segment_address,
              actual_segment->vmaddr() + actual_image->Slide());

    unsigned long expect_segment_size;
    const uint8_t* expect_segment_data = getsegmentdata(
        expect_image, segment_name.c_str(), &expect_segment_size);
    mach_vm_address_t expect_segment_address =
        reinterpret_cast<mach_vm_address_t>(expect_segment_data);
    EXPECT_EQ(expect_segment_address, actual_segment_address);
    EXPECT_EQ(expect_segment_size, actual_segment->vmsize());
    EXPECT_EQ(actual_segment->vmsize(), actual_segment_size);
  } else {
    // getsegmentdata() doesn’t return appropriate data for the __PAGEZERO
    // segment because getsegmentdata() always adjusts for slide, but the
    // __PAGEZERO segment never slides, it just grows. Skip the getsegmentdata()
    // check for that segment according to the same rules that the kernel uses
    // to identify __PAGEZERO. See 10.9.4 xnu-2422.110.17/bsd/kern/mach_loader.c
    // load_segment().
    EXPECT_EQ(actual_segment_address, actual_segment->vmaddr());
    EXPECT_EQ(actual_segment->vmsize() + actual_image->Slide(),
              actual_segment_size);
  }

  ASSERT_EQ(expect_segment->nsects, actual_segment->nsects());

  // Make sure that the expected load command is big enough for the number of
  // sections that it claims to have, and set up a pointer to its first section
  // structure.
  ASSERT_EQ(sizeof(*expect_segment) + expect_segment->nsects * sizeof(Section),
            expect_segment->cmdsize);
  const Section* expect_sections =
      reinterpret_cast<const Section*>(&expect_segment[1]);

  for (size_t index = 0; index < actual_segment->nsects(); ++index) {
    const Section* expect_section = &expect_sections[index];
    const process_types::section* actual_section =
        actual_segment->GetSectionAtIndex(index);
    ExpectSection(&expect_sections[index], actual_section);
    if (testing::Test::HasFatalFailure()) {
      return;
    }

    // Make sure that the section is accessible by GetSectionByName as well.
    std::string section_name =
        MachOImageSegmentReader::SectionNameString(expect_section->sectname);
    const process_types::section* actual_section_by_name =
        actual_segment->GetSectionByName(section_name);
    EXPECT_EQ(actual_section, actual_section_by_name);

    // Make sure that the section is accessible by the parent MachOImageReader’s
    // GetSectionByName.
    mach_vm_address_t actual_section_address;
    const process_types::section* actual_section_from_image_by_name =
        actual_image->GetSectionByName(
            segment_name, section_name, &actual_section_address);
    EXPECT_EQ(actual_section, actual_section_from_image_by_name);

    if (actual_segment->SegmentSlides()) {
      EXPECT_EQ(actual_section_address,
                actual_section->addr + actual_image->Slide());

      unsigned long expect_section_size;
      const uint8_t* expect_section_data = getsectiondata(expect_image,
                                                          segment_name.c_str(),
                                                          section_name.c_str(),
                                                          &expect_section_size);
      mach_vm_address_t expect_section_address =
          reinterpret_cast<mach_vm_address_t>(expect_section_data);
      EXPECT_EQ(expect_section_address, actual_section_address);
      EXPECT_EQ(expect_section_size, actual_section->size);
    } else {
      EXPECT_EQ(actual_section_address, actual_section->addr);
    }

    // Test the parent MachOImageReader’s GetSectionAtIndex as well.
    mach_vm_address_t actual_section_address_at_index;
    const process_types::section* actual_section_from_image_at_index =
        actual_image->GetSectionAtIndex(++(*section_index),
                                        &actual_section_address_at_index);
    EXPECT_EQ(actual_section, actual_section_from_image_at_index);
    EXPECT_EQ(actual_section_address, actual_section_address_at_index);
  }

  EXPECT_EQ(NULL, actual_segment->GetSectionByName("NoSuchSection"));
}

// Walks through the load commands of |expect_image|, finding all of the
// expected segment commands. For each expected segment command, calls
// actual_image->GetSegmentByName() to obtain an actual segment command, and
// calls ExpectSegmentCommand() to compare the expected and actual segments. A
// series of by-name lookups is also performed on the segment to ensure that it
// behaves correctly when attempting to look up segment and section names that
// are not present. |test_section_indices| should be true to test
// MachOImageReader::GetSectionAtIndex() using out-of-range section indices.
// This should be tested for at least one module, but it’s very noisy in terms
// of logging output, so this knob is provided to suppress this portion of the
// test when looping over all modules.
void ExpectSegmentCommands(const MachHeader* expect_image,
                           const MachOImageReader* actual_image,
                           bool test_section_index_bounds) {
  ASSERT_TRUE(expect_image);
  ASSERT_TRUE(actual_image);

  const char* commands_base = reinterpret_cast<const char*>(&expect_image[1]);
  uint32_t position = 0;
  size_t section_index = 0;
  for (uint32_t index = 0; index < expect_image->ncmds; ++index) {
    ASSERT_LT(position, expect_image->sizeofcmds);
    const load_command* command =
        reinterpret_cast<const load_command*>(&commands_base[position]);
    ASSERT_LE(position + command->cmdsize, expect_image->sizeofcmds);
    if (command->cmd == kSegmentCommand) {
      const SegmentCommand* expect_segment =
          reinterpret_cast<const SegmentCommand*>(command);
      std::string segment_name =
          MachOImageSegmentReader::SegmentNameString(expect_segment->segname);
      mach_vm_address_t actual_segment_address;
      mach_vm_size_t actual_segment_size;
      const MachOImageSegmentReader* actual_segment =
          actual_image->GetSegmentByName(
              segment_name, &actual_segment_address, &actual_segment_size);
      ExpectSegmentCommand(expect_segment,
                           expect_image,
                           actual_segment,
                           actual_segment_address,
                           actual_segment_size,
                           actual_image,
                           &section_index);
      if (testing::Test::HasFatalFailure()) {
        return;
      }
    }
    position += command->cmdsize;
  }
  EXPECT_EQ(expect_image->sizeofcmds, position);

  if (test_section_index_bounds) {
    // GetSectionAtIndex uses a 1-based index. Make sure that the range is
    // correct.
    EXPECT_EQ(NULL, actual_image->GetSectionAtIndex(0, NULL));
    EXPECT_EQ(NULL, actual_image->GetSectionAtIndex(section_index + 1, NULL));
  }

  // Make sure that by-name lookups for names that don’t exist work properly:
  // they should return NULL.
  EXPECT_FALSE(actual_image->GetSegmentByName("NoSuchSegment", NULL, NULL));
  EXPECT_FALSE(
      actual_image->GetSectionByName("NoSuchSegment", "NoSuchSection", NULL));

  // Make sure that there’s a __TEXT segment so that this can do a valid test of
  // a section that doesn’t exist within a segment that does.
  EXPECT_TRUE(actual_image->GetSegmentByName(SEG_TEXT, NULL, NULL));
  EXPECT_FALSE(actual_image->GetSectionByName(SEG_TEXT, "NoSuchSection", NULL));

  // Similarly, make sure that a section name that exists in one segment isn’t
  // accidentally found during a lookup for that section in a different segment.
  EXPECT_TRUE(actual_image->GetSectionByName(SEG_TEXT, SECT_TEXT, NULL));
  EXPECT_FALSE(
      actual_image->GetSectionByName("NoSuchSegment", SECT_TEXT, NULL));
  EXPECT_FALSE(actual_image->GetSectionByName(SEG_DATA, SECT_TEXT, NULL));

  // The __LINKEDIT segment normally does exist but doesn’t have any sections.
  EXPECT_FALSE(
      actual_image->GetSectionByName(SEG_LINKEDIT, "NoSuchSection", NULL));
  EXPECT_FALSE(actual_image->GetSectionByName(SEG_LINKEDIT, SECT_TEXT, NULL));
}

// Verifies that |expect_image| is a vaild Mach-O header for the current system
// by checking its |magic| and |cputype| fields. Then, verifies that the
// information in |actual_image| matches that in |expect_image|. The |filetype|
// field is examined, and actual_image->Address() is compared to
// |expect_image_address|. Various other attributes of |actual_image| are
// sanity-checked depending on the Mach-O file type. Finally,
// ExpectSegmentCommands() is called to verify all that all of the segments
// match; |test_section_index_bounds| is used as an argument to that function.
void ExpectMachImage(const MachHeader* expect_image,
                     mach_vm_address_t expect_image_address,
                     const MachOImageReader* actual_image,
                     bool test_section_index_bounds) {
  ASSERT_TRUE(expect_image);
  ASSERT_TRUE(actual_image);

  EXPECT_EQ(kMachMagic, expect_image->magic);
  EXPECT_EQ(kCPUType, expect_image->cputype);

  EXPECT_EQ(expect_image->filetype, actual_image->FileType());
  EXPECT_EQ(expect_image_address, actual_image->Address());

  mach_vm_address_t actual_text_segment_address;
  mach_vm_size_t actual_text_segment_size;
  const MachOImageSegmentReader* actual_text_segment =
      actual_image->GetSegmentByName(
          SEG_TEXT, &actual_text_segment_address, &actual_text_segment_size);
  ASSERT_TRUE(actual_text_segment);
  EXPECT_EQ(expect_image_address, actual_text_segment_address);
  EXPECT_EQ(actual_image->Size(), actual_text_segment_size);
  EXPECT_EQ(expect_image_address - actual_text_segment->vmaddr(),
            actual_image->Slide());

  uint32_t file_type = actual_image->FileType();
  EXPECT_TRUE(file_type == MH_EXECUTE || file_type == MH_DYLIB ||
              file_type == MH_DYLINKER || file_type == MH_BUNDLE);

  if (file_type == MH_EXECUTE || file_type == MH_DYLINKER) {
    EXPECT_EQ("/usr/lib/dyld", actual_image->DylinkerName());
  }

  // For these, just don’t crash or anything.
  if (file_type == MH_DYLIB) {
    actual_image->DylibVersion();
  }
  actual_image->SourceVersion();
  UUID uuid;
  actual_image->UUID(&uuid);

  ExpectSegmentCommands(expect_image, actual_image, test_section_index_bounds);
}

TEST(MachOImageReader, Self_MainExecutable) {
  ProcessReader process_reader;
  ASSERT_TRUE(process_reader.Initialize(mach_task_self()));

  const MachHeader* mh_execute_header = reinterpret_cast<MachHeader*>(
      dlsym(RTLD_MAIN_ONLY, "_mh_execute_header"));
  ASSERT_NE(static_cast<void*>(NULL), mh_execute_header);
  mach_vm_address_t mh_execute_header_address =
      reinterpret_cast<mach_vm_address_t>(mh_execute_header);

  MachOImageReader image_reader;
  ASSERT_TRUE(image_reader.Initialize(
      &process_reader, mh_execute_header_address, "mh_execute_header"));

  EXPECT_EQ(static_cast<uint32_t>(MH_EXECUTE), image_reader.FileType());

  ExpectMachImage(
      mh_execute_header, mh_execute_header_address, &image_reader, true);
}

TEST(MachOImageReader, Self_DyldImages) {
  ProcessReader process_reader;
  ASSERT_TRUE(process_reader.Initialize(mach_task_self()));

  const struct dyld_all_image_infos* dyld_image_infos =
      _dyld_get_all_image_infos();
  ASSERT_GE(dyld_image_infos->version, 1u);
  ASSERT_TRUE(dyld_image_infos->infoArray);

  for (uint32_t index = 0; index < dyld_image_infos->infoArrayCount; ++index) {
    const dyld_image_info* dyld_image = &dyld_image_infos->infoArray[index];
    SCOPED_TRACE(base::StringPrintf(
        "index %u, image %s", index, dyld_image->imageFilePath));

    // dyld_image_info::imageLoadAddress is poorly-declared: it’s declared as
    // const mach_header* in both 32-bit and 64-bit environments, but in the
    // 64-bit environment, it should be const mach_header_64*.
    const MachHeader* mach_header =
        reinterpret_cast<const MachHeader*>(dyld_image->imageLoadAddress);
    mach_vm_address_t image_address =
        reinterpret_cast<mach_vm_address_t>(mach_header);

    MachOImageReader image_reader;
    ASSERT_TRUE(image_reader.Initialize(
        &process_reader, image_address, dyld_image->imageFilePath));

    uint32_t file_type = image_reader.FileType();
    if (index == 0) {
      EXPECT_EQ(static_cast<uint32_t>(MH_EXECUTE), file_type);
    } else {
      EXPECT_TRUE(file_type == MH_DYLIB || file_type == MH_BUNDLE);
    }

    ExpectMachImage(mach_header, image_address, &image_reader, false);
    if (Test::HasFatalFailure()) {
      return;
    }
  }

  // Now that all of the modules have been verified, make sure that dyld itself
  // can be read properly too.
  if (dyld_image_infos->version >= 2) {
    SCOPED_TRACE("dyld");

    // dyld_all_image_infos::dyldImageLoadAddress is poorly-declared too.
    const MachHeader* mach_header = reinterpret_cast<const MachHeader*>(
        dyld_image_infos->dyldImageLoadAddress);
    mach_vm_address_t image_address =
        reinterpret_cast<mach_vm_address_t>(mach_header);

    MachOImageReader image_reader;
    ASSERT_TRUE(
        image_reader.Initialize(&process_reader, image_address, "dyld"));

    EXPECT_EQ(static_cast<uint32_t>(MH_DYLINKER), image_reader.FileType());

    ExpectMachImage(mach_header, image_address, &image_reader, false);
    if (Test::HasFatalFailure()) {
      return;
    }
  }

  // If dyld is new enough to record UUIDs, check the UUID of any module that
  // it says has one. Note that dyld doesn’t record UUIDs of anything that
  // loaded out of the shared cache, but it should at least have a UUID for the
  // main executable if it has one.
  if (dyld_image_infos->version >= 8 && dyld_image_infos->uuidArray) {
    for (uint32_t index = 0;
         index < dyld_image_infos->uuidArrayCount;
         ++index) {
      const dyld_uuid_info* dyld_image = &dyld_image_infos->uuidArray[index];
      SCOPED_TRACE(base::StringPrintf("uuid index %u", index));

      // dyld_uuid_info::imageLoadAddress is poorly-declared too.
      const MachHeader* mach_header =
          reinterpret_cast<const MachHeader*>(dyld_image->imageLoadAddress);
      mach_vm_address_t image_address =
          reinterpret_cast<mach_vm_address_t>(mach_header);

      MachOImageReader image_reader;
      ASSERT_TRUE(
          image_reader.Initialize(&process_reader, image_address, "uuid"));

      ExpectMachImage(mach_header, image_address, &image_reader, false);

      UUID expected_uuid;
      expected_uuid.InitializeFromBytes(dyld_image->imageUUID);
      UUID actual_uuid;
      image_reader.UUID(&actual_uuid);
      EXPECT_EQ(expected_uuid, actual_uuid);
    }
  }
}

}  // namespace
