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

#include "snapshot/mac/mach_o_image_segment_reader.h"

#include <mach-o/loader.h>

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "gtest/gtest.h"

namespace crashpad {
namespace test {
namespace {

// Most of MachOImageSegmentReader is tested as part of MachOImageReader, which
// depends on MachOImageSegmentReader to provide major portions of its
// functionality. Because MachOImageSegmentReader is difficult to use except by
// a Mach-O load command reader such as MachOImageReader, these portions
// of MachOImageSegmentReader are not tested independently.
//
// The tests here exercise the portions of MachOImageSegmentReader that are
// exposed and independently useful.

TEST(MachOImageSegmentReader, SegmentNameString) {
  // The output value should be a string of up to 16 characters, even if the
  // input value is not NUL-terminated within 16 characters.
  EXPECT_EQ("__TEXT", MachOImageSegmentReader::SegmentNameString("__TEXT"));
  EXPECT_EQ("__OVER", MachOImageSegmentReader::SegmentNameString("__OVER"));
  EXPECT_EQ("", MachOImageSegmentReader::SegmentNameString(""));
  EXPECT_EQ("p", MachOImageSegmentReader::SegmentNameString("p"));
  EXPECT_EQ("NoUnderChar",
            MachOImageSegmentReader::SegmentNameString("NoUnderChar"));
  EXPECT_EQ("0123456789abcde",
            MachOImageSegmentReader::SegmentNameString("0123456789abcde"));
  EXPECT_EQ("0123456789abcdef",
            MachOImageSegmentReader::SegmentNameString("0123456789abcdef"));
  EXPECT_EQ("gfedcba987654321",
            MachOImageSegmentReader::SegmentNameString("gfedcba9876543210"));
  EXPECT_EQ("hgfedcba98765432",
            MachOImageSegmentReader::SegmentNameString("hgfedcba9876543210"));

  // Segment names defined in <mach-o/loader.h>. All of these should come
  // through SegmentNameString() cleanly and without truncation.
  const char* kSegmentTestData[] = {
      SEG_TEXT,
      SEG_DATA,
      SEG_OBJC,
      SEG_ICON,
      SEG_LINKEDIT,
      SEG_UNIXSTACK,
      SEG_IMPORT,
  };

  for (size_t index = 0; index < arraysize(kSegmentTestData); ++index) {
    EXPECT_EQ(
        kSegmentTestData[index],
        MachOImageSegmentReader::SegmentNameString(kSegmentTestData[index]))
        << base::StringPrintf("index %zu", index);
  }
}

TEST(MachOImageSegmentReader, SectionNameString) {
  // The output value should be a string of up to 16 characters, even if the
  // input value is not NUL-terminated within 16 characters.
  EXPECT_EQ("__text", MachOImageSegmentReader::SectionNameString("__text"));
  EXPECT_EQ("__over", MachOImageSegmentReader::SectionNameString("__over"));
  EXPECT_EQ("", MachOImageSegmentReader::SectionNameString(""));
  EXPECT_EQ("p", MachOImageSegmentReader::SectionNameString("p"));
  EXPECT_EQ("NoUnderChar",
            MachOImageSegmentReader::SectionNameString("NoUnderChar"));
  EXPECT_EQ("0123456789abcde",
            MachOImageSegmentReader::SectionNameString("0123456789abcde"));
  EXPECT_EQ("0123456789abcdef",
            MachOImageSegmentReader::SectionNameString("0123456789abcdef"));
  EXPECT_EQ("gfedcba987654321",
            MachOImageSegmentReader::SectionNameString("gfedcba9876543210"));
  EXPECT_EQ("hgfedcba98765432",
            MachOImageSegmentReader::SectionNameString("hgfedcba9876543210"));

  // Section names defined in <mach-o/loader.h>. All of these should come
  // through SectionNameString() cleanly and without truncation.
  const char* kSectionTestData[] = {
      SECT_TEXT,
      SECT_FVMLIB_INIT0,
      SECT_FVMLIB_INIT1,
      SECT_DATA,
      SECT_BSS,
      SECT_COMMON,
      SECT_OBJC_SYMBOLS,
      SECT_OBJC_MODULES,
      SECT_OBJC_STRINGS,
      SECT_OBJC_REFS,
      SECT_ICON_HEADER,
      SECT_ICON_TIFF,
  };

  for (size_t index = 0; index < arraysize(kSectionTestData); ++index) {
    EXPECT_EQ(
        kSectionTestData[index],
        MachOImageSegmentReader::SectionNameString(kSectionTestData[index]))
        << base::StringPrintf("index %zu", index);
  }
}

TEST(MachOImageSegmentReader, SegmentAndSectionNameString) {
  struct SegmentAndSectionTestData {
    const char* segment;
    const char* section;
    const char* output;
  };
  const SegmentAndSectionTestData kSegmentAndSectionTestData[] = {
      {"segment", "section", "segment,section"},
      {"Segment", "Section", "Segment,Section"},
      {"SEGMENT", "SECTION", "SEGMENT,SECTION"},
      {"__TEXT", "__plain", "__TEXT,__plain"},
      {"__TEXT", "poetry", "__TEXT,poetry"},
      {"__TEXT", "Prose", "__TEXT,Prose"},
      {"__PLAIN", "__text", "__PLAIN,__text"},
      {"rich", "__text", "rich,__text"},
      {"segment", "", "segment,"},
      {"", "section", ",section"},
      {"", "", ","},
      {"0123456789abcdef", "section", "0123456789abcdef,section"},
      {"gfedcba9876543210", "section", "gfedcba987654321,section"},
      {"0123456789abcdef", "", "0123456789abcdef,"},
      {"gfedcba9876543210", "", "gfedcba987654321,"},
      {"segment", "0123456789abcdef", "segment,0123456789abcdef"},
      {"segment", "gfedcba9876543210", "segment,gfedcba987654321"},
      {"", "0123456789abcdef", ",0123456789abcdef"},
      {"", "gfedcba9876543210", ",gfedcba987654321"},
      {"0123456789abcdef",
       "0123456789abcdef",
       "0123456789abcdef,0123456789abcdef"},
      {"gfedcba9876543210",
       "gfedcba9876543210",
       "gfedcba987654321,gfedcba987654321"},

      // Sections defined in <mach-o/loader.h>. All of these should come through
      // SegmentAndSectionNameString() cleanly and without truncation.
      {SEG_TEXT, SECT_TEXT, "__TEXT,__text"},
      {SEG_TEXT, SECT_FVMLIB_INIT0, "__TEXT,__fvmlib_init0"},
      {SEG_TEXT, SECT_FVMLIB_INIT1, "__TEXT,__fvmlib_init1"},
      {SEG_DATA, SECT_DATA, "__DATA,__data"},
      {SEG_DATA, SECT_BSS, "__DATA,__bss"},
      {SEG_DATA, SECT_COMMON, "__DATA,__common"},
      {SEG_OBJC, SECT_OBJC_SYMBOLS, "__OBJC,__symbol_table"},
      {SEG_OBJC, SECT_OBJC_MODULES, "__OBJC,__module_info"},
      {SEG_OBJC, SECT_OBJC_STRINGS, "__OBJC,__selector_strs"},
      {SEG_OBJC, SECT_OBJC_REFS, "__OBJC,__selector_refs"},
      {SEG_ICON, SECT_ICON_HEADER, "__ICON,__header"},
      {SEG_ICON, SECT_ICON_TIFF, "__ICON,__tiff"},

      // These segments don’t normally have sections, but the above group tested
      // the known segment names for segments that do normally have sections.
      // This group does the same for segments that normally don’t.
      {SEG_LINKEDIT, "", "__LINKEDIT,"},
      {SEG_UNIXSTACK, "", "__UNIXSTACK,"},
      {SEG_IMPORT, "", "__IMPORT,"},
  };

  for (size_t index = 0; index < arraysize(kSegmentAndSectionTestData);
       ++index) {
    const SegmentAndSectionTestData& test = kSegmentAndSectionTestData[index];
    EXPECT_EQ(test.output,
              MachOImageSegmentReader::SegmentAndSectionNameString(
                  test.segment, test.section))
        << base::StringPrintf("index %zu, segment %s, section %s",
                              index,
                              test.segment,
                              test.section);
  }
}

}  // namespace
}  // namespace test
}  // namespace crashpad
