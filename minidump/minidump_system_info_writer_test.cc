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

#include "minidump/minidump_system_info_writer.h"

#include <dbghelp.h>
#include <string.h>

#include <string>

#include "gtest/gtest.h"
#include "minidump/minidump_file_writer.h"
#include "minidump/minidump_file_writer_test_util.h"
#include "util/file/string_file_writer.h"

namespace crashpad {
namespace test {
namespace {

void GetSystemInfoStream(const std::string& file_contents,
                         size_t csd_version_length,
                         const MINIDUMP_SYSTEM_INFO** system_info,
                         const MINIDUMP_STRING** csd_version) {
  // The expected number of bytes for the CSD version’s MINIDUMP_STRING::Buffer.
  const size_t kCSDVersionBytes =
      csd_version_length * sizeof(MINIDUMP_STRING::Buffer[0]);
  const size_t kCSDVersionBytesWithNUL =
      kCSDVersionBytes + sizeof(MINIDUMP_STRING::Buffer[0]);

  const size_t kDirectoryOffset = sizeof(MINIDUMP_HEADER);
  const size_t kSystemInfoStreamOffset =
      kDirectoryOffset + sizeof(MINIDUMP_DIRECTORY);
  const size_t kCSDVersionOffset =
      kSystemInfoStreamOffset + sizeof(MINIDUMP_SYSTEM_INFO);
  const size_t kFileSize =
      kCSDVersionOffset + sizeof(MINIDUMP_STRING) + kCSDVersionBytesWithNUL;

  ASSERT_EQ(kFileSize, file_contents.size());

  const MINIDUMP_HEADER* header =
      reinterpret_cast<const MINIDUMP_HEADER*>(&file_contents[0]);

  ASSERT_NO_FATAL_FAILURE(VerifyMinidumpHeader(header, 1, 0));

  const MINIDUMP_DIRECTORY* directory =
      reinterpret_cast<const MINIDUMP_DIRECTORY*>(
          &file_contents[kDirectoryOffset]);

  ASSERT_EQ(kMinidumpStreamTypeSystemInfo, directory->StreamType);
  ASSERT_EQ(sizeof(MINIDUMP_SYSTEM_INFO), directory->Location.DataSize);
  ASSERT_EQ(kSystemInfoStreamOffset, directory->Location.Rva);

  *system_info = reinterpret_cast<const MINIDUMP_SYSTEM_INFO*>(
      &file_contents[kSystemInfoStreamOffset]);

  ASSERT_EQ(kCSDVersionOffset, (*system_info)->CSDVersionRva);

  *csd_version = reinterpret_cast<const MINIDUMP_STRING*>(
      &file_contents[kCSDVersionOffset]);

  ASSERT_EQ(kCSDVersionBytes, (*csd_version)->Length);
}

TEST(MinidumpSystemInfoWriter, Empty) {
  MinidumpFileWriter minidump_file_writer;
  MinidumpSystemInfoWriter system_info_writer;

  system_info_writer.SetCSDVersion(std::string());

  minidump_file_writer.AddStream(&system_info_writer);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&file_writer));

  const MINIDUMP_SYSTEM_INFO* system_info;
  const MINIDUMP_STRING* csd_version;

  ASSERT_NO_FATAL_FAILURE(
      GetSystemInfoStream(file_writer.string(), 0, &system_info, &csd_version));

  EXPECT_EQ(kMinidumpCPUArchitectureUnknown,
            system_info->ProcessorArchitecture);
  EXPECT_EQ(0u, system_info->ProcessorLevel);
  EXPECT_EQ(0u, system_info->ProcessorRevision);
  EXPECT_EQ(0u, system_info->NumberOfProcessors);
  EXPECT_EQ(0u, system_info->ProductType);
  EXPECT_EQ(0u, system_info->MajorVersion);
  EXPECT_EQ(0u, system_info->MinorVersion);
  EXPECT_EQ(0u, system_info->BuildNumber);
  EXPECT_EQ(0u, system_info->PlatformId);
  EXPECT_EQ(0u, system_info->SuiteMask);
  EXPECT_EQ(0u, system_info->Cpu.X86CpuInfo.VendorId[0]);
  EXPECT_EQ(0u, system_info->Cpu.X86CpuInfo.VendorId[1]);
  EXPECT_EQ(0u, system_info->Cpu.X86CpuInfo.VendorId[2]);
  EXPECT_EQ(0u, system_info->Cpu.X86CpuInfo.VersionInformation);
  EXPECT_EQ(0u, system_info->Cpu.X86CpuInfo.FeatureInformation);
  EXPECT_EQ(0u, system_info->Cpu.X86CpuInfo.AMDExtendedCpuFeatures);
  EXPECT_EQ(0u, system_info->Cpu.OtherCpuInfo.ProcessorFeatures[0]);
  EXPECT_EQ(0u, system_info->Cpu.OtherCpuInfo.ProcessorFeatures[1]);

  EXPECT_EQ('\0', csd_version->Buffer[0]);
}

TEST(MinidumpSystemInfoWriter, X86_Win) {
  MinidumpFileWriter minidump_file_writer;
  MinidumpSystemInfoWriter system_info_writer;

  const MinidumpCPUArchitecture kCPUArchitecture = kMinidumpCPUArchitectureX86;
  const uint16_t kCPULevel = 0x0010;
  const uint16_t kCPURevision = 0x0602;
  const uint8_t kCPUCount = 1;
  const MinidumpOS kOS = kMinidumpOSWin32NT;
  const MinidumpOSType kOSType = kMinidumpOSTypeWorkstation;
  const uint32_t kOSVersionMajor = 6;
  const uint32_t kOSVersionMinor = 1;
  const uint32_t kOSVersionBuild = 7601;
  const char kCSDVersion[] = "Service Pack 1";
  const uint16_t kSuiteMask = VER_SUITE_SINGLEUSERTS;
  const char kCPUVendor[] = "AuthenticAMD";
  const uint32_t kCPUVersion = 0x00100f62;
  const uint32_t kCPUFeatures = 0x078bfbff;
  const uint32_t kAMDFeatures = 0xefd3fbff;

  uint32_t cpu_vendor_registers[3];
  ASSERT_EQ(sizeof(cpu_vendor_registers), strlen(kCPUVendor));
  memcpy(cpu_vendor_registers, kCPUVendor, sizeof(cpu_vendor_registers));

  system_info_writer.SetCPUArchitecture(kCPUArchitecture);
  system_info_writer.SetCPULevelAndRevision(kCPULevel, kCPURevision);
  system_info_writer.SetCPUCount(kCPUCount);
  system_info_writer.SetOS(kOS);
  system_info_writer.SetOSType(kMinidumpOSTypeWorkstation);
  system_info_writer.SetOSVersion(
      kOSVersionMajor, kOSVersionMinor, kOSVersionBuild);
  system_info_writer.SetCSDVersion(kCSDVersion);
  system_info_writer.SetSuiteMask(kSuiteMask);
  system_info_writer.SetCPUX86VendorString(kCPUVendor);
  system_info_writer.SetCPUX86VersionAndFeatures(kCPUVersion, kCPUFeatures);
  system_info_writer.SetCPUX86AMDExtendedFeatures(kAMDFeatures);

  minidump_file_writer.AddStream(&system_info_writer);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&file_writer));

  const MINIDUMP_SYSTEM_INFO* system_info;
  const MINIDUMP_STRING* csd_version;

  ASSERT_NO_FATAL_FAILURE(GetSystemInfoStream(
      file_writer.string(), strlen(kCSDVersion), &system_info, &csd_version));

  EXPECT_EQ(kCPUArchitecture, system_info->ProcessorArchitecture);
  EXPECT_EQ(kCPULevel, system_info->ProcessorLevel);
  EXPECT_EQ(kCPURevision, system_info->ProcessorRevision);
  EXPECT_EQ(kCPUCount, system_info->NumberOfProcessors);
  EXPECT_EQ(kOSType, system_info->ProductType);
  EXPECT_EQ(kOSVersionMajor, system_info->MajorVersion);
  EXPECT_EQ(kOSVersionMinor, system_info->MinorVersion);
  EXPECT_EQ(kOSVersionBuild, system_info->BuildNumber);
  EXPECT_EQ(kOS, system_info->PlatformId);
  EXPECT_EQ(kSuiteMask, system_info->SuiteMask);
  EXPECT_EQ(cpu_vendor_registers[0], system_info->Cpu.X86CpuInfo.VendorId[0]);
  EXPECT_EQ(cpu_vendor_registers[1], system_info->Cpu.X86CpuInfo.VendorId[1]);
  EXPECT_EQ(cpu_vendor_registers[2], system_info->Cpu.X86CpuInfo.VendorId[2]);
  EXPECT_EQ(kCPUVersion, system_info->Cpu.X86CpuInfo.VersionInformation);
  EXPECT_EQ(kCPUFeatures, system_info->Cpu.X86CpuInfo.FeatureInformation);
  EXPECT_EQ(kAMDFeatures, system_info->Cpu.X86CpuInfo.AMDExtendedCpuFeatures);

  for (size_t index = 0; index < strlen(kCSDVersion); ++index) {
    EXPECT_EQ(kCSDVersion[index], csd_version->Buffer[index]) << index;
  }
}

TEST(MinidumpSystemInfoWriter, X86_64_Mac) {
  MinidumpFileWriter minidump_file_writer;
  MinidumpSystemInfoWriter system_info_writer;

  const MinidumpCPUArchitecture kCPUArchitecture =
      kMinidumpCPUArchitectureAMD64;
  const uint16_t kCPULevel = 0x0006;
  const uint16_t kCPURevision = 0x3a09;
  const uint8_t kCPUCount = 8;
  const MinidumpOS kOS = kMinidumpOSMacOSX;
  const MinidumpOSType kOSType = kMinidumpOSTypeWorkstation;
  const uint32_t kOSVersionMajor = 10;
  const uint32_t kOSVersionMinor = 9;
  const uint32_t kOSVersionBuild = 4;
  const char kCSDVersion[] = "13E28";
  const uint64_t kCPUFeatures[2] = {0x10427f4c, 0x00000000};

  system_info_writer.SetCPUArchitecture(kCPUArchitecture);
  system_info_writer.SetCPULevelAndRevision(kCPULevel, kCPURevision);
  system_info_writer.SetCPUCount(kCPUCount);
  system_info_writer.SetOS(kOS);
  system_info_writer.SetOSType(kMinidumpOSTypeWorkstation);
  system_info_writer.SetOSVersion(
      kOSVersionMajor, kOSVersionMinor, kOSVersionBuild);
  system_info_writer.SetCSDVersion(kCSDVersion);
  system_info_writer.SetCPUOtherFeatures(kCPUFeatures[0], kCPUFeatures[1]);

  minidump_file_writer.AddStream(&system_info_writer);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&file_writer));

  const MINIDUMP_SYSTEM_INFO* system_info;
  const MINIDUMP_STRING* csd_version;

  ASSERT_NO_FATAL_FAILURE(GetSystemInfoStream(
      file_writer.string(), strlen(kCSDVersion), &system_info, &csd_version));

  EXPECT_EQ(kCPUArchitecture, system_info->ProcessorArchitecture);
  EXPECT_EQ(kCPULevel, system_info->ProcessorLevel);
  EXPECT_EQ(kCPURevision, system_info->ProcessorRevision);
  EXPECT_EQ(kCPUCount, system_info->NumberOfProcessors);
  EXPECT_EQ(kOSType, system_info->ProductType);
  EXPECT_EQ(kOSVersionMajor, system_info->MajorVersion);
  EXPECT_EQ(kOSVersionMinor, system_info->MinorVersion);
  EXPECT_EQ(kOSVersionBuild, system_info->BuildNumber);
  EXPECT_EQ(kOS, system_info->PlatformId);
  EXPECT_EQ(0u, system_info->SuiteMask);
  EXPECT_EQ(kCPUFeatures[0],
            system_info->Cpu.OtherCpuInfo.ProcessorFeatures[0]);
  EXPECT_EQ(kCPUFeatures[1],
            system_info->Cpu.OtherCpuInfo.ProcessorFeatures[1]);
}

TEST(MinidumpSystemInfoWriter, X86_CPUVendorFromRegisters) {
  // MinidumpSystemInfoWriter.X86_Win already tested SetCPUX86VendorString().
  // This test exercises SetCPUX86Vendor() to set the vendor from register
  // values.
  MinidumpFileWriter minidump_file_writer;
  MinidumpSystemInfoWriter system_info_writer;

  const MinidumpCPUArchitecture kCPUArchitecture = kMinidumpCPUArchitectureX86;
  const uint32_t kCPUVendor[] = {'uneG', 'Ieni', 'letn'};

  system_info_writer.SetCPUArchitecture(kCPUArchitecture);
  system_info_writer.SetCPUX86Vendor(
      kCPUVendor[0], kCPUVendor[1], kCPUVendor[2]);
  system_info_writer.SetCSDVersion(std::string());

  minidump_file_writer.AddStream(&system_info_writer);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&file_writer));

  const MINIDUMP_SYSTEM_INFO* system_info;
  const MINIDUMP_STRING* csd_version;

  ASSERT_NO_FATAL_FAILURE(
      GetSystemInfoStream(file_writer.string(), 0, &system_info, &csd_version));

  EXPECT_EQ(kCPUArchitecture, system_info->ProcessorArchitecture);
  EXPECT_EQ(0u, system_info->ProcessorLevel);
  EXPECT_EQ(kCPUVendor[0], system_info->Cpu.X86CpuInfo.VendorId[0]);
  EXPECT_EQ(kCPUVendor[1], system_info->Cpu.X86CpuInfo.VendorId[1]);
  EXPECT_EQ(kCPUVendor[2], system_info->Cpu.X86CpuInfo.VendorId[2]);
  EXPECT_EQ(0u, system_info->Cpu.X86CpuInfo.VersionInformation);
}

TEST(MinidumpSystemInfoWriterDeathTest, NoCSDVersion) {
  MinidumpFileWriter minidump_file_writer;
  MinidumpSystemInfoWriter system_info_writer;
  minidump_file_writer.AddStream(&system_info_writer);

  StringFileWriter file_writer;
  ASSERT_DEATH(minidump_file_writer.WriteEverything(&file_writer),
               "csd_version_");
}

}  // namespace
}  // namespace test
}  // namespace crashpad
