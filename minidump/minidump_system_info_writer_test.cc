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

#include <string.h>

#include <algorithm>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "gtest/gtest.h"
#include "minidump/minidump_file_writer.h"
#include "minidump/test/minidump_file_writer_test_util.h"
#include "minidump/test/minidump_string_writer_test_util.h"
#include "minidump/test/minidump_writable_test_util.h"
#include "snapshot/test/test_system_snapshot.h"
#include "test/gtest_death_check.h"
#include "util/file/string_file.h"

namespace crashpad {
namespace test {
namespace {

void GetSystemInfoStream(const std::string& file_contents,
                         size_t csd_version_length,
                         const MINIDUMP_SYSTEM_INFO** system_info,
                         const MINIDUMP_STRING** csd_version) {
  // The expected number of bytes for the CSD version’s MINIDUMP_STRING::Buffer.
  MINIDUMP_STRING tmp = {0};
  ALLOW_UNUSED_LOCAL(tmp);
  const size_t kCSDVersionBytes = csd_version_length * sizeof(tmp.Buffer[0]);
  const size_t kCSDVersionBytesWithNUL =
      kCSDVersionBytes + sizeof(tmp.Buffer[0]);

  const size_t kDirectoryOffset = sizeof(MINIDUMP_HEADER);
  const size_t kSystemInfoStreamOffset =
      kDirectoryOffset + sizeof(MINIDUMP_DIRECTORY);
  const size_t kCSDVersionOffset =
      kSystemInfoStreamOffset + sizeof(MINIDUMP_SYSTEM_INFO);
  const size_t kFileSize =
      kCSDVersionOffset + sizeof(MINIDUMP_STRING) + kCSDVersionBytesWithNUL;

  ASSERT_EQ(kFileSize, file_contents.size());

  const MINIDUMP_DIRECTORY* directory;
  const MINIDUMP_HEADER* header =
      MinidumpHeaderAtStart(file_contents, &directory);
  ASSERT_NO_FATAL_FAILURE(VerifyMinidumpHeader(header, 1, 0));
  ASSERT_TRUE(directory);

  ASSERT_EQ(kMinidumpStreamTypeSystemInfo, directory[0].StreamType);
  EXPECT_EQ(kSystemInfoStreamOffset, directory[0].Location.Rva);

  *system_info = MinidumpWritableAtLocationDescriptor<MINIDUMP_SYSTEM_INFO>(
      file_contents, directory[0].Location);
  ASSERT_TRUE(system_info);

  EXPECT_EQ(kCSDVersionOffset, (*system_info)->CSDVersionRva);

  *csd_version =
      MinidumpStringAtRVA(file_contents, (*system_info)->CSDVersionRva);
  EXPECT_EQ(kCSDVersionBytes, (*csd_version)->Length);
}

TEST(MinidumpSystemInfoWriter, Empty) {
  MinidumpFileWriter minidump_file_writer;
  auto system_info_writer = make_scoped_ptr(new MinidumpSystemInfoWriter());

  system_info_writer->SetCSDVersion(std::string());

  minidump_file_writer.AddStream(std::move(system_info_writer));

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  const MINIDUMP_SYSTEM_INFO* system_info = nullptr;
  const MINIDUMP_STRING* csd_version = nullptr;

  ASSERT_NO_FATAL_FAILURE(
      GetSystemInfoStream(string_file.string(), 0, &system_info, &csd_version));

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
  auto system_info_writer = make_scoped_ptr(new MinidumpSystemInfoWriter());

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

  system_info_writer->SetCPUArchitecture(kCPUArchitecture);
  system_info_writer->SetCPULevelAndRevision(kCPULevel, kCPURevision);
  system_info_writer->SetCPUCount(kCPUCount);
  system_info_writer->SetOS(kOS);
  system_info_writer->SetOSType(kMinidumpOSTypeWorkstation);
  system_info_writer->SetOSVersion(
      kOSVersionMajor, kOSVersionMinor, kOSVersionBuild);
  system_info_writer->SetCSDVersion(kCSDVersion);
  system_info_writer->SetSuiteMask(kSuiteMask);
  system_info_writer->SetCPUX86VendorString(kCPUVendor);
  system_info_writer->SetCPUX86VersionAndFeatures(kCPUVersion, kCPUFeatures);
  system_info_writer->SetCPUX86AMDExtendedFeatures(kAMDFeatures);

  minidump_file_writer.AddStream(std::move(system_info_writer));

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  const MINIDUMP_SYSTEM_INFO* system_info = nullptr;
  const MINIDUMP_STRING* csd_version = nullptr;

  ASSERT_NO_FATAL_FAILURE(GetSystemInfoStream(
      string_file.string(), strlen(kCSDVersion), &system_info, &csd_version));

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

TEST(MinidumpSystemInfoWriter, AMD64_Mac) {
  MinidumpFileWriter minidump_file_writer;
  auto system_info_writer = make_scoped_ptr(new MinidumpSystemInfoWriter());

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

  system_info_writer->SetCPUArchitecture(kCPUArchitecture);
  system_info_writer->SetCPULevelAndRevision(kCPULevel, kCPURevision);
  system_info_writer->SetCPUCount(kCPUCount);
  system_info_writer->SetOS(kOS);
  system_info_writer->SetOSType(kMinidumpOSTypeWorkstation);
  system_info_writer->SetOSVersion(
      kOSVersionMajor, kOSVersionMinor, kOSVersionBuild);
  system_info_writer->SetCSDVersion(kCSDVersion);
  system_info_writer->SetCPUOtherFeatures(kCPUFeatures[0], kCPUFeatures[1]);

  minidump_file_writer.AddStream(std::move(system_info_writer));

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  const MINIDUMP_SYSTEM_INFO* system_info = nullptr;
  const MINIDUMP_STRING* csd_version;

  ASSERT_NO_FATAL_FAILURE(GetSystemInfoStream(
      string_file.string(), strlen(kCSDVersion), &system_info, &csd_version));

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
  auto system_info_writer = make_scoped_ptr(new MinidumpSystemInfoWriter());

  const MinidumpCPUArchitecture kCPUArchitecture = kMinidumpCPUArchitectureX86;
  const uint32_t kCPUVendor[] = {'uneG', 'Ieni', 'letn'};

  system_info_writer->SetCPUArchitecture(kCPUArchitecture);
  system_info_writer->SetCPUX86Vendor(
      kCPUVendor[0], kCPUVendor[1], kCPUVendor[2]);
  system_info_writer->SetCSDVersion(std::string());

  minidump_file_writer.AddStream(std::move(system_info_writer));

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  const MINIDUMP_SYSTEM_INFO* system_info = nullptr;
  const MINIDUMP_STRING* csd_version;

  ASSERT_NO_FATAL_FAILURE(
      GetSystemInfoStream(string_file.string(), 0, &system_info, &csd_version));

  EXPECT_EQ(kCPUArchitecture, system_info->ProcessorArchitecture);
  EXPECT_EQ(0u, system_info->ProcessorLevel);
  EXPECT_EQ(kCPUVendor[0], system_info->Cpu.X86CpuInfo.VendorId[0]);
  EXPECT_EQ(kCPUVendor[1], system_info->Cpu.X86CpuInfo.VendorId[1]);
  EXPECT_EQ(kCPUVendor[2], system_info->Cpu.X86CpuInfo.VendorId[2]);
  EXPECT_EQ(0u, system_info->Cpu.X86CpuInfo.VersionInformation);
}

TEST(MinidumpSystemInfoWriter, InitializeFromSnapshot_X86) {
  MINIDUMP_SYSTEM_INFO expect_system_info = {};

  const uint16_t kCPUFamily = 6;
  const uint8_t kCPUModel = 70;
  const uint8_t kCPUStepping = 1;

  const uint8_t kCPUBasicFamily =
      static_cast<uint8_t>(std::min(kCPUFamily, static_cast<uint16_t>(15)));
  const uint8_t kCPUExtendedFamily = kCPUFamily - kCPUBasicFamily;

  // These checks ensure that even if the constants above change, they represent
  // something that can legitimately be encoded in the form used by cpuid 1 eax.
  EXPECT_LE(kCPUFamily, 270);
  EXPECT_LE(kCPUStepping, 15);
  EXPECT_TRUE(kCPUBasicFamily == 6 || kCPUBasicFamily == 15 || kCPUModel <= 15);

  const uint8_t kCPUBasicModel = kCPUModel & 0xf;
  const uint8_t kCPUExtendedModel = kCPUModel >> 4;
  const uint32_t kCPUSignature =
      (kCPUExtendedFamily << 20) | (kCPUExtendedModel << 16) |
      (kCPUBasicFamily << 8) | (kCPUBasicModel << 4) | kCPUStepping;
  const uint64_t kCPUX86Features = 0x7ffafbffbfebfbff;
  expect_system_info.ProcessorArchitecture = kMinidumpCPUArchitectureX86;
  expect_system_info.ProcessorLevel = kCPUFamily;
  expect_system_info.ProcessorRevision = (kCPUModel << 8) | kCPUStepping;
  expect_system_info.NumberOfProcessors = 8;
  expect_system_info.ProductType = kMinidumpOSTypeServer;
  expect_system_info.MajorVersion = 10;
  expect_system_info.MinorVersion = 9;
  expect_system_info.BuildNumber = 5;
  expect_system_info.PlatformId = kMinidumpOSMacOSX;
  expect_system_info.SuiteMask = 0;
  expect_system_info.Cpu.X86CpuInfo.VendorId[0] = 'uneG';
  expect_system_info.Cpu.X86CpuInfo.VendorId[1] = 'Ieni';
  expect_system_info.Cpu.X86CpuInfo.VendorId[2] = 'letn';
  expect_system_info.Cpu.X86CpuInfo.VersionInformation = kCPUSignature;
  expect_system_info.Cpu.X86CpuInfo.FeatureInformation =
      kCPUX86Features & 0xffffffff;
  const char kCPUVendor[] = "GenuineIntel";
  const char kOSVersionBuild[] = "13F34";

  TestSystemSnapshot system_snapshot;
  system_snapshot.SetCPUArchitecture(kCPUArchitectureX86);
  system_snapshot.SetCPURevision(
      (kCPUFamily << 16) | (kCPUModel << 8) | kCPUStepping);
  system_snapshot.SetCPUCount(expect_system_info.NumberOfProcessors);
  system_snapshot.SetCPUVendor(kCPUVendor);
  system_snapshot.SetCPUX86Signature(kCPUSignature);
  system_snapshot.SetCPUX86Features(kCPUX86Features);
  system_snapshot.SetOperatingSystem(SystemSnapshot::kOperatingSystemMacOSX);
  system_snapshot.SetOSServer(true);
  system_snapshot.SetOSVersion(expect_system_info.MajorVersion,
                               expect_system_info.MinorVersion,
                               expect_system_info.BuildNumber,
                               kOSVersionBuild);

  auto system_info_writer = make_scoped_ptr(new MinidumpSystemInfoWriter());
  system_info_writer->InitializeFromSnapshot(&system_snapshot);

  MinidumpFileWriter minidump_file_writer;
  minidump_file_writer.AddStream(std::move(system_info_writer));

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  const MINIDUMP_SYSTEM_INFO* system_info = nullptr;
  const MINIDUMP_STRING* csd_version = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetSystemInfoStream(string_file.string(),
                                              strlen(kOSVersionBuild),
                                              &system_info,
                                              &csd_version));

  EXPECT_EQ(expect_system_info.ProcessorArchitecture,
            system_info->ProcessorArchitecture);
  EXPECT_EQ(expect_system_info.ProcessorLevel, system_info->ProcessorLevel);
  EXPECT_EQ(expect_system_info.ProcessorRevision,
            system_info->ProcessorRevision);
  EXPECT_EQ(expect_system_info.NumberOfProcessors,
            system_info->NumberOfProcessors);
  EXPECT_EQ(expect_system_info.ProductType, system_info->ProductType);
  EXPECT_EQ(expect_system_info.MajorVersion, system_info->MajorVersion);
  EXPECT_EQ(expect_system_info.MinorVersion, system_info->MinorVersion);
  EXPECT_EQ(expect_system_info.BuildNumber, system_info->BuildNumber);
  EXPECT_EQ(expect_system_info.PlatformId, system_info->PlatformId);
  EXPECT_EQ(expect_system_info.SuiteMask, system_info->SuiteMask);
  EXPECT_EQ(expect_system_info.Cpu.X86CpuInfo.VendorId[0],
            system_info->Cpu.X86CpuInfo.VendorId[0]);
  EXPECT_EQ(expect_system_info.Cpu.X86CpuInfo.VendorId[1],
            system_info->Cpu.X86CpuInfo.VendorId[1]);
  EXPECT_EQ(expect_system_info.Cpu.X86CpuInfo.VendorId[2],
            system_info->Cpu.X86CpuInfo.VendorId[2]);
  EXPECT_EQ(expect_system_info.Cpu.X86CpuInfo.VersionInformation,
            system_info->Cpu.X86CpuInfo.VersionInformation);
  EXPECT_EQ(expect_system_info.Cpu.X86CpuInfo.FeatureInformation,
            system_info->Cpu.X86CpuInfo.FeatureInformation);

  for (size_t index = 0; index < strlen(kOSVersionBuild); ++index) {
    EXPECT_EQ(kOSVersionBuild[index], csd_version->Buffer[index]) << index;
  }
}

TEST(MinidumpSystemInfoWriter, InitializeFromSnapshot_AMD64) {
  MINIDUMP_SYSTEM_INFO expect_system_info = {};

  const uint8_t kCPUFamily = 6;
  const uint8_t kCPUModel = 70;
  const uint8_t kCPUStepping = 1;
  expect_system_info.ProcessorArchitecture = kMinidumpCPUArchitectureAMD64;
  expect_system_info.ProcessorLevel = kCPUFamily;
  expect_system_info.ProcessorRevision = (kCPUModel << 8) | kCPUStepping;
  expect_system_info.NumberOfProcessors = 8;
  expect_system_info.ProductType = kMinidumpOSTypeServer;
  expect_system_info.MajorVersion = 10;
  expect_system_info.MinorVersion = 9;
  expect_system_info.BuildNumber = 5;
  expect_system_info.PlatformId = kMinidumpOSMacOSX;
  expect_system_info.SuiteMask = 0;
  expect_system_info.Cpu.OtherCpuInfo.ProcessorFeatures[0] =
      (1 << PF_COMPARE_EXCHANGE_DOUBLE) |
      (1 << PF_MMX_INSTRUCTIONS_AVAILABLE) |
      (1 << PF_XMMI_INSTRUCTIONS_AVAILABLE) |
      (1 << PF_RDTSC_INSTRUCTION_AVAILABLE) |
      (1 << PF_PAE_ENABLED) |
      (1 << PF_XMMI64_INSTRUCTIONS_AVAILABLE) |
      (1 << PF_SSE_DAZ_MODE_AVAILABLE) |
      (1 << PF_NX_ENABLED) |
      (1 << PF_SSE3_INSTRUCTIONS_AVAILABLE) |
      (1 << PF_COMPARE_EXCHANGE128) |
      (1 << PF_XSAVE_ENABLED) |
      (1 << PF_RDWRFSGSBASE_AVAILABLE) |
      (1 << PF_RDRAND_INSTRUCTION_AVAILABLE);
  expect_system_info.Cpu.OtherCpuInfo.ProcessorFeatures[1] = 0;
  const char kOSVersionBuild[] = "13F34";

  TestSystemSnapshot system_snapshot;
  system_snapshot.SetCPUArchitecture(kCPUArchitectureX86_64);
  system_snapshot.SetCPURevision(
      (kCPUFamily << 16) | (kCPUModel << 8) | kCPUStepping);
  system_snapshot.SetCPUCount(expect_system_info.NumberOfProcessors);
  system_snapshot.SetCPUX86Features(0x7ffafbffbfebfbff);
  system_snapshot.SetCPUX86ExtendedFeatures(0x000000212c100900);
  system_snapshot.SetCPUX86Leaf7Features(0x00002fbb);
  system_snapshot.SetCPUX86SupportsDAZ(true);
  system_snapshot.SetOperatingSystem(SystemSnapshot::kOperatingSystemMacOSX);
  system_snapshot.SetOSServer(true);
  system_snapshot.SetOSVersion(expect_system_info.MajorVersion,
                               expect_system_info.MinorVersion,
                               expect_system_info.BuildNumber,
                               kOSVersionBuild);
  system_snapshot.SetNXEnabled(true);

  auto system_info_writer = make_scoped_ptr(new MinidumpSystemInfoWriter());
  system_info_writer->InitializeFromSnapshot(&system_snapshot);

  MinidumpFileWriter minidump_file_writer;
  minidump_file_writer.AddStream(std::move(system_info_writer));

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  const MINIDUMP_SYSTEM_INFO* system_info = nullptr;
  const MINIDUMP_STRING* csd_version = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetSystemInfoStream(string_file.string(),
                                              strlen(kOSVersionBuild),
                                              &system_info,
                                              &csd_version));

  EXPECT_EQ(expect_system_info.ProcessorArchitecture,
            system_info->ProcessorArchitecture);
  EXPECT_EQ(expect_system_info.ProcessorLevel, system_info->ProcessorLevel);
  EXPECT_EQ(expect_system_info.ProcessorRevision,
            system_info->ProcessorRevision);
  EXPECT_EQ(expect_system_info.NumberOfProcessors,
            system_info->NumberOfProcessors);
  EXPECT_EQ(expect_system_info.ProductType, system_info->ProductType);
  EXPECT_EQ(expect_system_info.MajorVersion, system_info->MajorVersion);
  EXPECT_EQ(expect_system_info.MinorVersion, system_info->MinorVersion);
  EXPECT_EQ(expect_system_info.BuildNumber, system_info->BuildNumber);
  EXPECT_EQ(expect_system_info.PlatformId, system_info->PlatformId);
  EXPECT_EQ(expect_system_info.SuiteMask, system_info->SuiteMask);
  EXPECT_EQ(expect_system_info.Cpu.OtherCpuInfo.ProcessorFeatures[0],
            system_info->Cpu.OtherCpuInfo.ProcessorFeatures[0]);
  EXPECT_EQ(expect_system_info.Cpu.OtherCpuInfo.ProcessorFeatures[1],
            system_info->Cpu.OtherCpuInfo.ProcessorFeatures[1]);

  for (size_t index = 0; index < strlen(kOSVersionBuild); ++index) {
    EXPECT_EQ(kOSVersionBuild[index], csd_version->Buffer[index]) << index;
  }
}

TEST(MinidumpSystemInfoWriterDeathTest, NoCSDVersion) {
  MinidumpFileWriter minidump_file_writer;
  auto system_info_writer = make_scoped_ptr(new MinidumpSystemInfoWriter());
  minidump_file_writer.AddStream(std::move(system_info_writer));

  StringFile string_file;
  ASSERT_DEATH_CHECK(minidump_file_writer.WriteEverything(&string_file),
                     "csd_version_");
}

}  // namespace
}  // namespace test
}  // namespace crashpad
