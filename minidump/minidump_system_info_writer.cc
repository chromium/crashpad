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

#include "base/logging.h"
#include "minidump/minidump_string_writer.h"

namespace crashpad {

MinidumpSystemInfoWriter::MinidumpSystemInfoWriter()
    : MinidumpStreamWriter(), system_info_(), csd_version_() {
  system_info_.ProcessorArchitecture = kMinidumpCPUArchitectureUnknown;
}

MinidumpSystemInfoWriter::~MinidumpSystemInfoWriter() {
}

void MinidumpSystemInfoWriter::SetCSDVersion(const std::string& csd_version) {
  DCHECK_EQ(state(), kStateMutable);

  if (!csd_version_) {
    csd_version_.reset(new internal::MinidumpUTF16StringWriter());
  }

  csd_version_->SetUTF8(csd_version);
}

void MinidumpSystemInfoWriter::SetCPUX86Vendor(uint32_t ebx,
                                               uint32_t edx,
                                               uint32_t ecx) {
  DCHECK_EQ(state(), kStateMutable);
  DCHECK(system_info_.ProcessorArchitecture == kMinidumpCPUArchitectureX86 ||
         system_info_.ProcessorArchitecture ==
             kMinidumpCPUArchitectureX86Win64);

  COMPILE_ASSERT(arraysize(system_info_.Cpu.X86CpuInfo.VendorId) == 3,
                 vendor_id_must_have_3_elements);

  system_info_.Cpu.X86CpuInfo.VendorId[0] = ebx;
  system_info_.Cpu.X86CpuInfo.VendorId[1] = edx;
  system_info_.Cpu.X86CpuInfo.VendorId[2] = ecx;
}

void MinidumpSystemInfoWriter::SetCPUX86VendorString(
    const std::string& vendor) {
  DCHECK_EQ(state(), kStateMutable);
  CHECK_EQ(vendor.size(), sizeof(system_info_.Cpu.X86CpuInfo.VendorId));

  uint32_t registers[3];
  COMPILE_ASSERT(
      sizeof(registers) == sizeof(system_info_.Cpu.X86CpuInfo.VendorId),
      vendor_id_sizes_must_be_equal);

  for (size_t index = 0; index < arraysize(registers); ++index) {
    memcpy(&registers[index],
           &vendor[index * sizeof(*registers)],
           sizeof(*registers));
  }

  SetCPUX86Vendor(registers[0], registers[1], registers[2]);
}

void MinidumpSystemInfoWriter::SetCPUX86VersionAndFeatures(uint32_t version,
                                                           uint32_t features) {
  DCHECK_EQ(state(), kStateMutable);
  DCHECK(system_info_.ProcessorArchitecture == kMinidumpCPUArchitectureX86 ||
         system_info_.ProcessorArchitecture ==
             kMinidumpCPUArchitectureX86Win64);

  system_info_.Cpu.X86CpuInfo.VersionInformation = version;
  system_info_.Cpu.X86CpuInfo.FeatureInformation = features;
}

void MinidumpSystemInfoWriter::SetCPUX86AMDExtendedFeatures(
    uint32_t extended_features) {
  DCHECK_EQ(state(), kStateMutable);
  DCHECK(system_info_.ProcessorArchitecture == kMinidumpCPUArchitectureX86 ||
         system_info_.ProcessorArchitecture ==
             kMinidumpCPUArchitectureX86Win64);
  DCHECK(system_info_.Cpu.X86CpuInfo.VendorId[0] == 'htuA' &&
         system_info_.Cpu.X86CpuInfo.VendorId[1] == 'itne' &&
         system_info_.Cpu.X86CpuInfo.VendorId[2] == 'DMAc');

  system_info_.Cpu.X86CpuInfo.AMDExtendedCpuFeatures = extended_features;
}

void MinidumpSystemInfoWriter::SetCPUOtherFeatures(uint64_t features_0,
                                                   uint64_t features_1) {
  DCHECK_EQ(state(), kStateMutable);
  DCHECK(system_info_.ProcessorArchitecture != kMinidumpCPUArchitectureX86 &&
         system_info_.ProcessorArchitecture !=
             kMinidumpCPUArchitectureX86Win64);

  COMPILE_ASSERT(
      arraysize(system_info_.Cpu.OtherCpuInfo.ProcessorFeatures) == 2,
      processor_features_must_have_2_elements);

  system_info_.Cpu.OtherCpuInfo.ProcessorFeatures[0] = features_0;
  system_info_.Cpu.OtherCpuInfo.ProcessorFeatures[1] = features_1;
}

bool MinidumpSystemInfoWriter::Freeze() {
  DCHECK_EQ(state(), kStateMutable);
  CHECK(csd_version_);

  if (!MinidumpStreamWriter::Freeze()) {
    return false;
  }

  csd_version_->RegisterRVA(&system_info_.CSDVersionRva);

  return true;
}

size_t MinidumpSystemInfoWriter::SizeOfObject() {
  DCHECK_GE(state(), kStateFrozen);

  return sizeof(system_info_);
}

std::vector<internal::MinidumpWritable*> MinidumpSystemInfoWriter::Children() {
  DCHECK_GE(state(), kStateFrozen);
  DCHECK(csd_version_);

  std::vector<MinidumpWritable*> children(1, csd_version_.get());
  return children;
}

bool MinidumpSystemInfoWriter::WriteObject(FileWriterInterface* file_writer) {
  DCHECK_EQ(state(), kStateWritable);

  return file_writer->Write(&system_info_, sizeof(system_info_));
}

MinidumpStreamType MinidumpSystemInfoWriter::StreamType() const {
  return kMinidumpStreamTypeSystemInfo;
}

}  // namespace crashpad
