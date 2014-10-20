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

#include "minidump/minidump_module_writer.h"

#include <dbghelp.h>
#include <stdint.h>
#include <string.h>

#include "base/strings/utf_string_conversions.h"
#include "gtest/gtest.h"
#include "minidump/minidump_extensions.h"
#include "minidump/minidump_file_writer.h"
#include "minidump/test/minidump_file_writer_test_util.h"
#include "util/file/string_file_writer.h"
#include "util/misc/uuid.h"

namespace crashpad {
namespace test {
namespace {

void GetModuleListStream(const std::string& file_contents,
                         const MINIDUMP_MODULE_LIST** module_list) {
  const size_t kDirectoryOffset = sizeof(MINIDUMP_HEADER);
  const size_t kModuleListStreamOffset =
      kDirectoryOffset + sizeof(MINIDUMP_DIRECTORY);
  const size_t kModulesOffset =
      kModuleListStreamOffset + sizeof(MINIDUMP_MODULE_LIST);

  ASSERT_GE(file_contents.size(), kModulesOffset);

  const MINIDUMP_HEADER* header =
      reinterpret_cast<const MINIDUMP_HEADER*>(&file_contents[0]);

  ASSERT_NO_FATAL_FAILURE(VerifyMinidumpHeader(header, 1, 0));

  const MINIDUMP_DIRECTORY* directory =
      reinterpret_cast<const MINIDUMP_DIRECTORY*>(
          &file_contents[kDirectoryOffset]);

  ASSERT_EQ(kMinidumpStreamTypeModuleList, directory->StreamType);
  ASSERT_GE(directory->Location.DataSize, sizeof(MINIDUMP_MODULE_LIST));
  ASSERT_EQ(kModuleListStreamOffset, directory->Location.Rva);

  *module_list = reinterpret_cast<const MINIDUMP_MODULE_LIST*>(
      &file_contents[kModuleListStreamOffset]);

  ASSERT_EQ(sizeof(MINIDUMP_MODULE_LIST) +
                (*module_list)->NumberOfModules * sizeof(MINIDUMP_MODULE),
            directory->Location.DataSize);
}

TEST(MinidumpModuleWriter, EmptyModuleList) {
  MinidumpFileWriter minidump_file_writer;
  MinidumpModuleListWriter module_list_writer;

  minidump_file_writer.AddStream(&module_list_writer);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&file_writer));

  ASSERT_EQ(sizeof(MINIDUMP_HEADER) + sizeof(MINIDUMP_DIRECTORY) +
                sizeof(MINIDUMP_MODULE_LIST),
            file_writer.string().size());

  const MINIDUMP_MODULE_LIST* module_list;
  ASSERT_NO_FATAL_FAILURE(
      GetModuleListStream(file_writer.string(), &module_list));

  EXPECT_EQ(0u, module_list->NumberOfModules);
}

// If |expected_pdb_name| is not nullptr, |codeview_record| is used to locate a
// CodeView record in |file_contents|, and its fields are compared against the
// the |expected_pdb_*| values. If |expected_pdb_uuid| is supplied, the CodeView
// record must be a PDB 7.0 link, otherwise, it must be a PDB 2.0 link. If
// |expected_pdb_name| is nullptr, |codeview_record| must not point to anything.
void ExpectCodeViewRecord(const MINIDUMP_LOCATION_DESCRIPTOR* codeview_record,
                          const std::string& file_contents,
                          const char* expected_pdb_name,
                          const UUID* expected_pdb_uuid,
                          time_t expected_pdb_timestamp,
                          uint32_t expected_pdb_age) {
  if (expected_pdb_name) {
    EXPECT_NE(0u, codeview_record->Rva);
    ASSERT_LE(codeview_record->Rva + codeview_record->DataSize,
              file_contents.size());

    std::string observed_pdb_name;
    if (expected_pdb_uuid) {
      // The CodeView record should be a PDB 7.0 link.
      EXPECT_GE(codeview_record->DataSize,
                sizeof(MinidumpModuleCodeViewRecordPDB70));
      const MinidumpModuleCodeViewRecordPDB70* codeview_pdb70_record =
          reinterpret_cast<const MinidumpModuleCodeViewRecordPDB70*>(
              &file_contents[codeview_record->Rva]);
      EXPECT_EQ(MinidumpModuleCodeViewRecordPDB70::kSignature,
                codeview_pdb70_record->signature);
      EXPECT_EQ(0,
                memcmp(expected_pdb_uuid,
                       &codeview_pdb70_record->uuid,
                       sizeof(codeview_pdb70_record->uuid)));
      EXPECT_EQ(expected_pdb_age, codeview_pdb70_record->age);

      observed_pdb_name.assign(
          reinterpret_cast<const char*>(&codeview_pdb70_record->pdb_name[0]),
          codeview_record->DataSize -
              offsetof(MinidumpModuleCodeViewRecordPDB70, pdb_name));
    } else {
      // The CodeView record should be a PDB 2.0 link.
      EXPECT_GE(codeview_record->DataSize,
                sizeof(MinidumpModuleCodeViewRecordPDB20));
      const MinidumpModuleCodeViewRecordPDB20* codeview_pdb20_record =
          reinterpret_cast<const MinidumpModuleCodeViewRecordPDB20*>(
              &file_contents[codeview_record->Rva]);
      EXPECT_EQ(MinidumpModuleCodeViewRecordPDB20::kSignature,
                codeview_pdb20_record->signature);
      EXPECT_EQ(static_cast<uint32_t>(expected_pdb_timestamp),
                codeview_pdb20_record->timestamp);
      EXPECT_EQ(expected_pdb_age, codeview_pdb20_record->age);

      observed_pdb_name.assign(
          reinterpret_cast<const char*>(&codeview_pdb20_record->pdb_name[0]),
          codeview_record->DataSize -
              offsetof(MinidumpModuleCodeViewRecordPDB20, pdb_name));
    }

    // Check for, and then remove, the NUL terminator.
    EXPECT_EQ('\0', observed_pdb_name[observed_pdb_name.size() - 1]);
    observed_pdb_name.resize(observed_pdb_name.size() - 1);

    EXPECT_EQ(expected_pdb_name, observed_pdb_name);
  } else {
    // There should be no CodeView record.
    EXPECT_EQ(0u, codeview_record->DataSize);
    EXPECT_EQ(0u, codeview_record->Rva);
  }
}

// If |expected_debug_name| is not nullptr, |misc_record| is used to locate a
// miscellanous debugging record in |file_contents|, and its fields are compared
// against the the |expected_debug_*| values. If |expected_debug_name| is
// nullptr, |misc_record| must not point to anything.
void ExpectMiscellaneousDebugRecord(
    const MINIDUMP_LOCATION_DESCRIPTOR* misc_record,
    const std::string& file_contents,
    const char* expected_debug_name,
    uint32_t expected_debug_type,
    bool expected_debug_utf16) {
  if (expected_debug_name) {
    EXPECT_GE(misc_record->DataSize, sizeof(IMAGE_DEBUG_MISC));
    EXPECT_NE(0u, misc_record->Rva);
    ASSERT_LE(misc_record->Rva + misc_record->DataSize, file_contents.size());
    const IMAGE_DEBUG_MISC* misc_debug_record =
        reinterpret_cast<const IMAGE_DEBUG_MISC*>(
            &file_contents[misc_record->Rva]);
    EXPECT_EQ(expected_debug_type, misc_debug_record->DataType);
    EXPECT_EQ(misc_record->DataSize, misc_debug_record->Length);
    EXPECT_EQ(expected_debug_utf16, misc_debug_record->Unicode);
    EXPECT_EQ(0u, misc_debug_record->Reserved[0]);
    EXPECT_EQ(0u, misc_debug_record->Reserved[1]);
    EXPECT_EQ(0u, misc_debug_record->Reserved[2]);

    // Check for the NUL terminator.
    size_t bytes_available =
        misc_debug_record->Length - offsetof(IMAGE_DEBUG_MISC, Data);
    EXPECT_EQ('\0', misc_debug_record->Data[bytes_available - 1]);
    std::string observed_data(
        reinterpret_cast<const char*>(misc_debug_record->Data));

    size_t bytes_used;
    if (misc_debug_record->Unicode) {
      string16 observed_data_utf16(
          reinterpret_cast<const char16*>(misc_debug_record->Data));
      bytes_used = (observed_data_utf16.size() + 1) * sizeof(char16);
      observed_data = base::UTF16ToUTF8(observed_data_utf16);
    } else {
      observed_data = reinterpret_cast<const char*>(misc_debug_record->Data);
      bytes_used = (observed_data.size() + 1) * sizeof(char);
    }
    EXPECT_LE(bytes_used, bytes_available);

    // Make sure that any padding bytes after the first NUL are also NUL.
    for (size_t index = bytes_used; index < bytes_available; ++index) {
      EXPECT_EQ('\0', misc_debug_record->Data[index]);
    }

    EXPECT_EQ(expected_debug_name, observed_data);
  } else {
    // There should be no miscellaneous debugging record.
    EXPECT_EQ(0u, misc_record->DataSize);
    EXPECT_EQ(0u, misc_record->Rva);
  }
}

// ExpectModule() verifies that |expected| matches |observed|. Fields that are
// supposed to contain constant magic numbers are verified against the expected
// constants instead of |expected|. Reserved fields are verified to be 0. RVA
// and MINIDUMP_LOCATION_DESCRIPTOR fields are not verified against |expected|.
// Instead, |ModuleNameRva| is used to locate the module name, which is compared
// against |expected_module_name|. ExpectCodeViewRecord() and
// ExpectMiscellaneousDebugRecord() are used to verify the |CvRecord| and
// |MiscRecord| fields against |expected_pdb_*| and |expected_debug_*|
// parameters, respectively.
void ExpectModule(const MINIDUMP_MODULE* expected,
                  const MINIDUMP_MODULE* observed,
                  const std::string& file_contents,
                  const std::string& expected_module_name,
                  const char* expected_pdb_name,
                  const UUID* expected_pdb_uuid,
                  time_t expected_pdb_timestamp,
                  uint32_t expected_pdb_age,
                  const char* expected_debug_name,
                  uint32_t expected_debug_type,
                  bool expected_debug_utf16) {
  EXPECT_EQ(expected->BaseOfImage, observed->BaseOfImage);
  EXPECT_EQ(expected->SizeOfImage, observed->SizeOfImage);
  EXPECT_EQ(expected->CheckSum, observed->CheckSum);
  EXPECT_EQ(expected->TimeDateStamp, observed->TimeDateStamp);
  EXPECT_EQ(static_cast<uint32_t>(VS_FFI_SIGNATURE),
            observed->VersionInfo.dwSignature);
  EXPECT_EQ(static_cast<uint32_t>(VS_FFI_STRUCVERSION),
            observed->VersionInfo.dwStrucVersion);
  EXPECT_EQ(expected->VersionInfo.dwFileVersionMS,
            observed->VersionInfo.dwFileVersionMS);
  EXPECT_EQ(expected->VersionInfo.dwFileVersionLS,
            observed->VersionInfo.dwFileVersionLS);
  EXPECT_EQ(expected->VersionInfo.dwProductVersionMS,
            observed->VersionInfo.dwProductVersionMS);
  EXPECT_EQ(expected->VersionInfo.dwProductVersionLS,
            observed->VersionInfo.dwProductVersionLS);
  EXPECT_EQ(expected->VersionInfo.dwFileFlagsMask,
            observed->VersionInfo.dwFileFlagsMask);
  EXPECT_EQ(expected->VersionInfo.dwFileFlags,
            observed->VersionInfo.dwFileFlags);
  EXPECT_EQ(expected->VersionInfo.dwFileOS, observed->VersionInfo.dwFileOS);
  EXPECT_EQ(expected->VersionInfo.dwFileType, observed->VersionInfo.dwFileType);
  EXPECT_EQ(expected->VersionInfo.dwFileSubtype,
            observed->VersionInfo.dwFileSubtype);
  EXPECT_EQ(expected->VersionInfo.dwFileDateMS,
            observed->VersionInfo.dwFileDateMS);
  EXPECT_EQ(expected->VersionInfo.dwFileDateLS,
            observed->VersionInfo.dwFileDateLS);
  EXPECT_EQ(0u, observed->Reserved0);
  EXPECT_EQ(0u, observed->Reserved1);

  EXPECT_NE(0u, observed->ModuleNameRva);
  ASSERT_LE(observed->ModuleNameRva,
            file_contents.size() - sizeof(MINIDUMP_STRING));
  const MINIDUMP_STRING* module_name = reinterpret_cast<const MINIDUMP_STRING*>(
      &file_contents[observed->ModuleNameRva]);
  ASSERT_LE(observed->ModuleNameRva + sizeof(MINIDUMP_STRING) +
                (module_name->Length + 1),
            file_contents.size());
  ASSERT_EQ(0u, module_name->Length % 2);
  string16 observed_module_name_utf16(
      reinterpret_cast<const char16*>(
          &file_contents[observed->ModuleNameRva + sizeof(MINIDUMP_STRING)]),
      module_name->Length / 2);
  string16 expected_module_name_utf16 = base::UTF8ToUTF16(expected_module_name);
  EXPECT_EQ(expected_module_name_utf16, observed_module_name_utf16);

  ASSERT_NO_FATAL_FAILURE(ExpectCodeViewRecord(&observed->CvRecord,
                                               file_contents,
                                               expected_pdb_name,
                                               expected_pdb_uuid,
                                               expected_pdb_timestamp,
                                               expected_pdb_age));

  ASSERT_NO_FATAL_FAILURE(ExpectMiscellaneousDebugRecord(&observed->MiscRecord,
                                                         file_contents,
                                                         expected_debug_name,
                                                         expected_debug_type,
                                                         expected_debug_utf16));
}

TEST(MinidumpModuleWriter, EmptyModule) {
  MinidumpFileWriter minidump_file_writer;
  MinidumpModuleListWriter module_list_writer;

  const char kModuleName[] = "test_executable";

  MinidumpModuleWriter module_writer;
  module_writer.SetName(kModuleName);

  module_list_writer.AddModule(&module_writer);
  minidump_file_writer.AddStream(&module_list_writer);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&file_writer));

  ASSERT_GT(file_writer.string().size(),
            sizeof(MINIDUMP_HEADER) + sizeof(MINIDUMP_DIRECTORY) +
                sizeof(MINIDUMP_MODULE_LIST) + 1 * sizeof(MINIDUMP_MODULE));

  const MINIDUMP_MODULE_LIST* module_list;
  ASSERT_NO_FATAL_FAILURE(
      GetModuleListStream(file_writer.string(), &module_list));

  EXPECT_EQ(1u, module_list->NumberOfModules);

  MINIDUMP_MODULE expected = {};
  ASSERT_NO_FATAL_FAILURE(ExpectModule(&expected,
                                       &module_list->Modules[0],
                                       file_writer.string(),
                                       kModuleName,
                                       nullptr,
                                       nullptr,
                                       0,
                                       0,
                                       nullptr,
                                       0,
                                       false));
}

TEST(MinidumpModuleWriter, OneModule) {
  MinidumpFileWriter minidump_file_writer;
  MinidumpModuleListWriter module_list_writer;

  const char kModuleName[] = "statically_linked";
  const uint64_t kModuleBase = 0x10da69000;
  const uint32_t kModuleSize = 0x1000;
  const uint32_t kChecksum = 0x76543210;
  const time_t kTimestamp = 0x386d4380;
  const uint32_t kFileVersionMS = 0x00010002;
  const uint32_t kFileVersionLS = 0x00030004;
  const uint32_t kProductVersionMS = 0x00050006;
  const uint32_t kProductVersionLS = 0x00070008;
  const uint32_t kFileFlagsMask = VS_FF_DEBUG | VS_FF_PRERELEASE |
                                  VS_FF_PATCHED | VS_FF_PRIVATEBUILD |
                                  VS_FF_INFOINFERRED | VS_FF_SPECIALBUILD;
  const uint32_t kFileFlags = VS_FF_PRIVATEBUILD | VS_FF_SPECIALBUILD;
  const uint32_t kFileOS = VOS_DOS;
  const uint32_t kFileType = VFT_DRV;
  const uint32_t kFileSubtype = VFT2_DRV_KEYBOARD;
  const char kPDBName[] = "statical.pdb";
  const uint8_t kPDBUUIDBytes[16] =
      {0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10,
       0x08, 0x19, 0x2a, 0x3b, 0x4c, 0x5d, 0x6e, 0x7f};
  UUID pdb_uuid;
  pdb_uuid.InitializeFromBytes(kPDBUUIDBytes);
  const uint32_t kPDBAge = 1;
  const uint32_t kDebugType = IMAGE_DEBUG_MISC_EXENAME;
  const char kDebugName[] = "statical.dbg";
  const bool kDebugUTF16 = false;

  MinidumpModuleWriter module_writer;
  module_writer.SetName(kModuleName);
  module_writer.SetImageBaseAddress(kModuleBase);
  module_writer.SetImageSize(kModuleSize);
  module_writer.SetChecksum(kChecksum);
  module_writer.SetTimestamp(kTimestamp);
  module_writer.SetFileVersion(kFileVersionMS >> 16,
                               kFileVersionMS & 0xffff,
                               kFileVersionLS >> 16,
                               kFileVersionLS & 0xffff);
  module_writer.SetProductVersion(kProductVersionMS >> 16,
                                  kProductVersionMS & 0xffff,
                                  kProductVersionLS >> 16,
                                  kProductVersionLS & 0xffff);
  module_writer.SetFileFlagsAndMask(kFileFlags, kFileFlagsMask);
  module_writer.SetFileOS(kFileOS);
  module_writer.SetFileTypeAndSubtype(kFileType, kFileSubtype);

  MinidumpModuleCodeViewRecordPDB70Writer codeview_pdb70_writer;
  codeview_pdb70_writer.SetPDBName(kPDBName);
  codeview_pdb70_writer.SetUUIDAndAge(pdb_uuid, kPDBAge);
  module_writer.SetCodeViewRecord(&codeview_pdb70_writer);

  MinidumpModuleMiscDebugRecordWriter misc_debug_writer;
  misc_debug_writer.SetDataType(kDebugType);
  misc_debug_writer.SetData(kDebugName, kDebugUTF16);
  module_writer.SetMiscDebugRecord(&misc_debug_writer);

  module_list_writer.AddModule(&module_writer);
  minidump_file_writer.AddStream(&module_list_writer);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&file_writer));

  ASSERT_GT(file_writer.string().size(),
            sizeof(MINIDUMP_HEADER) + sizeof(MINIDUMP_DIRECTORY) +
                sizeof(MINIDUMP_MODULE_LIST) + 1 * sizeof(MINIDUMP_MODULE));

  const MINIDUMP_MODULE_LIST* module_list;
  ASSERT_NO_FATAL_FAILURE(
      GetModuleListStream(file_writer.string(), &module_list));

  EXPECT_EQ(1u, module_list->NumberOfModules);

  MINIDUMP_MODULE expected = {};
  expected.BaseOfImage = kModuleBase;
  expected.SizeOfImage = kModuleSize;
  expected.CheckSum = kChecksum;
  expected.TimeDateStamp = kTimestamp;
  expected.VersionInfo.dwFileVersionMS = kFileVersionMS;
  expected.VersionInfo.dwFileVersionLS = kFileVersionLS;
  expected.VersionInfo.dwProductVersionMS = kProductVersionMS;
  expected.VersionInfo.dwProductVersionLS = kProductVersionLS;
  expected.VersionInfo.dwFileFlagsMask = kFileFlagsMask;
  expected.VersionInfo.dwFileFlags = kFileFlags;
  expected.VersionInfo.dwFileOS = kFileOS;
  expected.VersionInfo.dwFileType = kFileType;
  expected.VersionInfo.dwFileSubtype = kFileSubtype;

  ASSERT_NO_FATAL_FAILURE(ExpectModule(&expected,
                                       &module_list->Modules[0],
                                       file_writer.string(),
                                       kModuleName,
                                       kPDBName,
                                       &pdb_uuid,
                                       0,
                                       kPDBAge,
                                       kDebugName,
                                       kDebugType,
                                       kDebugUTF16));
}

TEST(MinidumpModuleWriter, OneModule_CodeViewUsesPDB20_MiscUsesUTF16) {
  // MinidumpModuleWriter.OneModule tested with a PDB 7.0 link as the CodeView
  // record and an IMAGE_DEBUG_MISC record in UTF-8. This test exercises the
  // alternatives, a PDB 2.0 link as the CodeView record and an IMAGE_DEBUG_MISC
  // record with UTF-16 data.
  MinidumpFileWriter minidump_file_writer;
  MinidumpModuleListWriter module_list_writer;

  const char kModuleName[] = "dinosaur";
  const char kPDBName[] = "d1n05.pdb";
  const time_t kPDBTimestamp = 0x386d4380;
  const uint32_t kPDBAge = 1;
  const uint32_t kDebugType = IMAGE_DEBUG_MISC_EXENAME;
  const char kDebugName[] = "d1n05.dbg";
  const bool kDebugUTF16 = true;

  MinidumpModuleWriter module_writer;
  module_writer.SetName(kModuleName);

  MinidumpModuleCodeViewRecordPDB20Writer codeview_pdb20_writer;
  codeview_pdb20_writer.SetPDBName(kPDBName);
  codeview_pdb20_writer.SetTimestampAndAge(kPDBTimestamp, kPDBAge);
  module_writer.SetCodeViewRecord(&codeview_pdb20_writer);

  MinidumpModuleMiscDebugRecordWriter misc_debug_writer;
  misc_debug_writer.SetDataType(kDebugType);
  misc_debug_writer.SetData(kDebugName, kDebugUTF16);
  module_writer.SetMiscDebugRecord(&misc_debug_writer);

  module_list_writer.AddModule(&module_writer);
  minidump_file_writer.AddStream(&module_list_writer);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&file_writer));

  ASSERT_GT(file_writer.string().size(),
            sizeof(MINIDUMP_HEADER) + sizeof(MINIDUMP_DIRECTORY) +
                sizeof(MINIDUMP_MODULE_LIST) + 1 * sizeof(MINIDUMP_MODULE));

  const MINIDUMP_MODULE_LIST* module_list;
  ASSERT_NO_FATAL_FAILURE(
      GetModuleListStream(file_writer.string(), &module_list));

  EXPECT_EQ(1u, module_list->NumberOfModules);

  MINIDUMP_MODULE expected = {};

  ASSERT_NO_FATAL_FAILURE(ExpectModule(&expected,
                                       &module_list->Modules[0],
                                       file_writer.string(),
                                       kModuleName,
                                       kPDBName,
                                       nullptr,
                                       kPDBTimestamp,
                                       kPDBAge,
                                       kDebugName,
                                       kDebugType,
                                       kDebugUTF16));
}

TEST(MinidumpModuleWriter, ThreeModules) {
  // As good exercise, this test uses three modules, one with a PDB 7.0 link as
  // its CodeView record, one with no CodeView record, and one with a PDB 2.0
  // link as its CodeView record.
  MinidumpFileWriter minidump_file_writer;
  MinidumpModuleListWriter module_list_writer;

  const char kModuleName0[] = "main";
  const uint64_t kModuleBase0 = 0x100101000;
  const uint32_t kModuleSize0 = 0xf000;
  const char kPDBName0[] = "main";
  const uint8_t kPDBUUIDBytes0[16] =
      {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x11,
       0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};
  UUID pdb_uuid_0;
  pdb_uuid_0.InitializeFromBytes(kPDBUUIDBytes0);
  const uint32_t kPDBAge0 = 0;

  const char kModuleName1[] = "ld.so";
  const uint64_t kModuleBase1 = 0x200202000;
  const uint32_t kModuleSize1 = 0x1e000;

  const char kModuleName2[] = "libc.so";
  const uint64_t kModuleBase2 = 0x300303000;
  const uint32_t kModuleSize2 = 0x2d000;
  const char kPDBName2[] = "libc.so";
  const time_t kPDBTimestamp2 = 0x386d4380;
  const uint32_t kPDBAge2 = 2;

  MinidumpModuleWriter module_writer_0;
  module_writer_0.SetName(kModuleName0);
  module_writer_0.SetImageBaseAddress(kModuleBase0);
  module_writer_0.SetImageSize(kModuleSize0);

  MinidumpModuleCodeViewRecordPDB70Writer codeview_pdb70_writer_0;
  codeview_pdb70_writer_0.SetPDBName(kPDBName0);
  codeview_pdb70_writer_0.SetUUIDAndAge(pdb_uuid_0, kPDBAge0);
  module_writer_0.SetCodeViewRecord(&codeview_pdb70_writer_0);

  module_list_writer.AddModule(&module_writer_0);

  MinidumpModuleWriter module_writer_1;
  module_writer_1.SetName(kModuleName1);
  module_writer_1.SetImageBaseAddress(kModuleBase1);
  module_writer_1.SetImageSize(kModuleSize1);

  module_list_writer.AddModule(&module_writer_1);

  MinidumpModuleWriter module_writer_2;
  module_writer_2.SetName(kModuleName2);
  module_writer_2.SetImageBaseAddress(kModuleBase2);
  module_writer_2.SetImageSize(kModuleSize2);

  MinidumpModuleCodeViewRecordPDB20Writer codeview_pdb70_writer_2;
  codeview_pdb70_writer_2.SetPDBName(kPDBName2);
  codeview_pdb70_writer_2.SetTimestampAndAge(kPDBTimestamp2, kPDBAge2);
  module_writer_2.SetCodeViewRecord(&codeview_pdb70_writer_2);

  module_list_writer.AddModule(&module_writer_2);

  minidump_file_writer.AddStream(&module_list_writer);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&file_writer));

  ASSERT_GT(file_writer.string().size(),
            sizeof(MINIDUMP_HEADER) + sizeof(MINIDUMP_DIRECTORY) +
                sizeof(MINIDUMP_MODULE_LIST) + 1 * sizeof(MINIDUMP_MODULE));

  const MINIDUMP_MODULE_LIST* module_list;
  ASSERT_NO_FATAL_FAILURE(
      GetModuleListStream(file_writer.string(), &module_list));

  EXPECT_EQ(3u, module_list->NumberOfModules);

  MINIDUMP_MODULE expected = {};

  {
    SCOPED_TRACE("module 0");

    expected.BaseOfImage = kModuleBase0;
    expected.SizeOfImage = kModuleSize0;

    ASSERT_NO_FATAL_FAILURE(ExpectModule(&expected,
                                         &module_list->Modules[0],
                                         file_writer.string(),
                                         kModuleName0,
                                         kPDBName0,
                                         &pdb_uuid_0,
                                         0,
                                         kPDBAge0,
                                         nullptr,
                                         0,
                                         false));
  }

  {
    SCOPED_TRACE("module 1");

    expected.BaseOfImage = kModuleBase1;
    expected.SizeOfImage = kModuleSize1;

    ASSERT_NO_FATAL_FAILURE(ExpectModule(&expected,
                                         &module_list->Modules[1],
                                         file_writer.string(),
                                         kModuleName1,
                                         nullptr,
                                         nullptr,
                                         0,
                                         0,
                                         nullptr,
                                         0,
                                         false));
  }

  {
    SCOPED_TRACE("module 2");

    expected.BaseOfImage = kModuleBase2;
    expected.SizeOfImage = kModuleSize2;

    ASSERT_NO_FATAL_FAILURE(ExpectModule(&expected,
                                         &module_list->Modules[2],
                                         file_writer.string(),
                                         kModuleName2,
                                         kPDBName2,
                                         nullptr,
                                         kPDBTimestamp2,
                                         kPDBAge2,
                                         nullptr,
                                         0,
                                         false));
  }
}

TEST(MinidumpSystemInfoWriterDeathTest, NoModuleName) {
  MinidumpFileWriter minidump_file_writer;
  MinidumpModuleListWriter module_list_writer;
  MinidumpModuleWriter module_writer;
  module_list_writer.AddModule(&module_writer);
  minidump_file_writer.AddStream(&module_list_writer);

  StringFileWriter file_writer;
  ASSERT_DEATH(minidump_file_writer.WriteEverything(&file_writer), "name_");
}

}  // namespace
}  // namespace test
}  // namespace crashpad
