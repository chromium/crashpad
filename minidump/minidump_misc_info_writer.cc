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

#include "minidump/minidump_misc_info_writer.h"

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "minidump/minidump_writer_util.h"
#include "util/file/file_writer.h"
#include "util/numeric/safe_assignment.h"

namespace crashpad {

MinidumpMiscInfoWriter::MinidumpMiscInfoWriter()
    : MinidumpStreamWriter(), misc_info_() {
}

MinidumpMiscInfoWriter::~MinidumpMiscInfoWriter() {
}

void MinidumpMiscInfoWriter::SetProcessId(uint32_t process_id) {
  DCHECK_EQ(state(), kStateMutable);

  misc_info_.ProcessId = process_id;
  misc_info_.Flags1 |= MINIDUMP_MISC1_PROCESS_ID;
}

void MinidumpMiscInfoWriter::SetProcessTimes(time_t process_create_time,
                                             uint32_t process_user_time,
                                             uint32_t process_kernel_time) {
  DCHECK_EQ(state(), kStateMutable);

  internal::MinidumpWriterUtil::AssignTimeT(&misc_info_.ProcessCreateTime,
                                            process_create_time);

  misc_info_.ProcessUserTime = process_user_time;
  misc_info_.ProcessKernelTime = process_kernel_time;
  misc_info_.Flags1 |= MINIDUMP_MISC1_PROCESS_TIMES;
}

void MinidumpMiscInfoWriter::SetProcessorPowerInfo(
    uint32_t processor_max_mhz,
    uint32_t processor_current_mhz,
    uint32_t processor_mhz_limit,
    uint32_t processor_max_idle_state,
    uint32_t processor_current_idle_state) {
  DCHECK_EQ(state(), kStateMutable);

  misc_info_.ProcessorMaxMhz = processor_max_mhz;
  misc_info_.ProcessorCurrentMhz = processor_current_mhz;
  misc_info_.ProcessorMhzLimit = processor_mhz_limit;
  misc_info_.ProcessorMaxIdleState = processor_max_idle_state;
  misc_info_.ProcessorCurrentIdleState = processor_current_idle_state;
  misc_info_.Flags1 |= MINIDUMP_MISC1_PROCESSOR_POWER_INFO;
}

void MinidumpMiscInfoWriter::SetProcessIntegrityLevel(
    uint32_t process_integrity_level) {
  DCHECK_EQ(state(), kStateMutable);

  misc_info_.ProcessIntegrityLevel = process_integrity_level;
  misc_info_.Flags1 |= MINIDUMP_MISC3_PROCESS_INTEGRITY;
}

void MinidumpMiscInfoWriter::SetProcessExecuteFlags(
    uint32_t process_execute_flags) {
  DCHECK_EQ(state(), kStateMutable);

  misc_info_.ProcessExecuteFlags = process_execute_flags;
  misc_info_.Flags1 |= MINIDUMP_MISC3_PROCESS_EXECUTE_FLAGS;
}

void MinidumpMiscInfoWriter::SetProtectedProcess(uint32_t protected_process) {
  DCHECK_EQ(state(), kStateMutable);

  misc_info_.ProtectedProcess = protected_process;
  misc_info_.Flags1 |= MINIDUMP_MISC3_PROTECTED_PROCESS;
}

void MinidumpMiscInfoWriter::SetTimeZone(uint32_t time_zone_id,
                                         int32_t bias,
                                         const std::string& standard_name,
                                         const SYSTEMTIME& standard_date,
                                         int32_t standard_bias,
                                         const std::string& daylight_name,
                                         const SYSTEMTIME& daylight_date,
                                         int32_t daylight_bias) {
  DCHECK_EQ(state(), kStateMutable);

  misc_info_.TimeZoneId = time_zone_id;
  misc_info_.TimeZone.Bias = bias;

  internal::MinidumpWriterUtil::AssignUTF8ToUTF16(
      misc_info_.TimeZone.StandardName,
      arraysize(misc_info_.TimeZone.StandardName),
      standard_name);

  misc_info_.TimeZone.StandardDate = standard_date;
  misc_info_.TimeZone.StandardBias = standard_bias;

  internal::MinidumpWriterUtil::AssignUTF8ToUTF16(
      misc_info_.TimeZone.DaylightName,
      arraysize(misc_info_.TimeZone.DaylightName),
      daylight_name);

  misc_info_.TimeZone.DaylightDate = daylight_date;
  misc_info_.TimeZone.DaylightBias = daylight_bias;

  misc_info_.Flags1 |= MINIDUMP_MISC3_TIMEZONE;
}

void MinidumpMiscInfoWriter::SetBuildString(
    const std::string& build_string,
    const std::string& debug_build_string) {
  DCHECK_EQ(state(), kStateMutable);

  misc_info_.Flags1 |= MINIDUMP_MISC4_BUILDSTRING;

  internal::MinidumpWriterUtil::AssignUTF8ToUTF16(
      misc_info_.BuildString, arraysize(misc_info_.BuildString), build_string);
  internal::MinidumpWriterUtil::AssignUTF8ToUTF16(
      misc_info_.DbgBldStr,
      arraysize(misc_info_.DbgBldStr),
      debug_build_string);
}

bool MinidumpMiscInfoWriter::Freeze() {
  DCHECK_EQ(state(), kStateMutable);

  if (!MinidumpStreamWriter::Freeze()) {
    return false;
  }

  size_t size = CalculateSizeOfObjectFromFlags();
  if (!AssignIfInRange(&misc_info_.SizeOfInfo, size)) {
    LOG(ERROR) << "size " << size << " out of range";
    return false;
  }

  return true;
}

size_t MinidumpMiscInfoWriter::SizeOfObject() {
  DCHECK_GE(state(), kStateFrozen);

  return CalculateSizeOfObjectFromFlags();
}

bool MinidumpMiscInfoWriter::WriteObject(FileWriterInterface* file_writer) {
  DCHECK_EQ(state(), kStateWritable);

  return file_writer->Write(&misc_info_, CalculateSizeOfObjectFromFlags());
}

MinidumpStreamType MinidumpMiscInfoWriter::StreamType() const {
  return kMinidumpStreamTypeMiscInfo;
}

size_t MinidumpMiscInfoWriter::CalculateSizeOfObjectFromFlags() const {
  DCHECK_GE(state(), kStateFrozen);

  if (misc_info_.Flags1 & MINIDUMP_MISC4_BUILDSTRING) {
    return sizeof(MINIDUMP_MISC_INFO_4);
  }
  if (misc_info_.Flags1 &
      (MINIDUMP_MISC3_PROCESS_INTEGRITY | MINIDUMP_MISC3_PROCESS_EXECUTE_FLAGS |
       MINIDUMP_MISC3_TIMEZONE | MINIDUMP_MISC3_PROTECTED_PROCESS)) {
    return sizeof(MINIDUMP_MISC_INFO_3);
  }
  if (misc_info_.Flags1 & MINIDUMP_MISC1_PROCESSOR_POWER_INFO) {
    return sizeof(MINIDUMP_MISC_INFO_2);
  }
  return sizeof(MINIDUMP_MISC_INFO);
}

}  // namespace crashpad
