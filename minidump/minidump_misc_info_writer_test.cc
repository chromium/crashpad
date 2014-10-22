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

#include <dbghelp.h>
#include <string.h>

#include <string>

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "gtest/gtest.h"
#include "minidump/minidump_file_writer.h"
#include "minidump/test/minidump_file_writer_test_util.h"
#include "minidump/test/minidump_writable_test_util.h"
#include "util/file/string_file_writer.h"
#include "util/stdlib/strlcpy.h"

namespace crashpad {
namespace test {
namespace {

template <typename T>
void GetMiscInfoStream(const std::string& file_contents, const T** misc_info) {
  const size_t kDirectoryOffset = sizeof(MINIDUMP_HEADER);
  const size_t kMiscInfoStreamOffset =
      kDirectoryOffset + sizeof(MINIDUMP_DIRECTORY);
  const size_t kMiscInfoStreamSize = sizeof(T);
  const size_t kFileSize = kMiscInfoStreamOffset + kMiscInfoStreamSize;

  ASSERT_EQ(kFileSize, file_contents.size());

  const MINIDUMP_DIRECTORY* directory;
  const MINIDUMP_HEADER* header =
      MinidumpHeaderAtStart(file_contents, &directory);
  ASSERT_NO_FATAL_FAILURE(VerifyMinidumpHeader(header, 1, 0));
  ASSERT_TRUE(directory);

  ASSERT_EQ(kMinidumpStreamTypeMiscInfo, directory[0].StreamType);
  EXPECT_EQ(kMiscInfoStreamOffset, directory[0].Location.Rva);

  *misc_info = MinidumpWritableAtLocationDescriptor<T>(file_contents,
                                                       directory[0].Location);
  ASSERT_TRUE(misc_info);
}

void ExpectNULPaddedString16Equal(const char16* expected,
                                  const char16* observed,
                                  size_t size) {
  string16 expected_string(expected, size);
  string16 observed_string(observed, size);
  EXPECT_EQ(expected_string, observed_string);
}

void ExpectSystemTimeEqual(const SYSTEMTIME* expected,
                           const SYSTEMTIME* observed) {
  EXPECT_EQ(expected->wYear, observed->wYear);
  EXPECT_EQ(expected->wMonth, observed->wMonth);
  EXPECT_EQ(expected->wDayOfWeek, observed->wDayOfWeek);
  EXPECT_EQ(expected->wDay, observed->wDay);
  EXPECT_EQ(expected->wHour, observed->wHour);
  EXPECT_EQ(expected->wMinute, observed->wMinute);
  EXPECT_EQ(expected->wSecond, observed->wSecond);
  EXPECT_EQ(expected->wMilliseconds, observed->wMilliseconds);
}

template <typename T>
void ExpectMiscInfoEqual(const T* expected, const T* observed);

template <>
void ExpectMiscInfoEqual<MINIDUMP_MISC_INFO>(
    const MINIDUMP_MISC_INFO* expected,
    const MINIDUMP_MISC_INFO* observed) {
  EXPECT_EQ(expected->Flags1, observed->Flags1);
  EXPECT_EQ(expected->ProcessId, observed->ProcessId);
  EXPECT_EQ(expected->ProcessCreateTime, observed->ProcessCreateTime);
  EXPECT_EQ(expected->ProcessUserTime, observed->ProcessUserTime);
  EXPECT_EQ(expected->ProcessKernelTime, observed->ProcessKernelTime);
}

template <>
void ExpectMiscInfoEqual<MINIDUMP_MISC_INFO_2>(
    const MINIDUMP_MISC_INFO_2* expected,
    const MINIDUMP_MISC_INFO_2* observed) {
  ExpectMiscInfoEqual<MINIDUMP_MISC_INFO>(expected, observed);
  EXPECT_EQ(expected->ProcessorMaxMhz, observed->ProcessorMaxMhz);
  EXPECT_EQ(expected->ProcessorCurrentMhz, observed->ProcessorCurrentMhz);
  EXPECT_EQ(expected->ProcessorMhzLimit, observed->ProcessorMhzLimit);
  EXPECT_EQ(expected->ProcessorMaxIdleState, observed->ProcessorMaxIdleState);
  EXPECT_EQ(expected->ProcessorCurrentIdleState,
            observed->ProcessorCurrentIdleState);
}

template <>
void ExpectMiscInfoEqual<MINIDUMP_MISC_INFO_3>(
    const MINIDUMP_MISC_INFO_3* expected,
    const MINIDUMP_MISC_INFO_3* observed) {
  ExpectMiscInfoEqual<MINIDUMP_MISC_INFO_2>(expected, observed);
  EXPECT_EQ(expected->ProcessIntegrityLevel, observed->ProcessIntegrityLevel);
  EXPECT_EQ(expected->ProcessExecuteFlags, observed->ProcessExecuteFlags);
  EXPECT_EQ(expected->ProtectedProcess, observed->ProtectedProcess);
  EXPECT_EQ(expected->TimeZoneId, observed->TimeZoneId);
  EXPECT_EQ(expected->TimeZone.Bias, observed->TimeZone.Bias);
  {
    SCOPED_TRACE("Standard");
    ExpectNULPaddedString16Equal(expected->TimeZone.StandardName,
                                 observed->TimeZone.StandardName,
                                 arraysize(expected->TimeZone.StandardName));
    ExpectSystemTimeEqual(&expected->TimeZone.StandardDate,
                          &observed->TimeZone.StandardDate);
    EXPECT_EQ(expected->TimeZone.StandardBias, observed->TimeZone.StandardBias);
  }
  {
    SCOPED_TRACE("Daylight");
    ExpectNULPaddedString16Equal(expected->TimeZone.DaylightName,
                                 observed->TimeZone.DaylightName,
                                 arraysize(expected->TimeZone.DaylightName));
    ExpectSystemTimeEqual(&expected->TimeZone.DaylightDate,
                          &observed->TimeZone.DaylightDate);
    EXPECT_EQ(expected->TimeZone.DaylightBias, observed->TimeZone.DaylightBias);
  }
}

template <>
void ExpectMiscInfoEqual<MINIDUMP_MISC_INFO_4>(
    const MINIDUMP_MISC_INFO_4* expected,
    const MINIDUMP_MISC_INFO_4* observed) {
  ExpectMiscInfoEqual<MINIDUMP_MISC_INFO_3>(expected, observed);
  {
    SCOPED_TRACE("BuildString");
    ExpectNULPaddedString16Equal(expected->BuildString,
                                 observed->BuildString,
                                 arraysize(expected->BuildString));
  }
  {
    SCOPED_TRACE("DbgBldStr");
    ExpectNULPaddedString16Equal(expected->DbgBldStr,
                                 observed->DbgBldStr,
                                 arraysize(expected->DbgBldStr));
  }
}

TEST(MinidumpMiscInfoWriter, Empty) {
  MinidumpFileWriter minidump_file_writer;
  MinidumpMiscInfoWriter misc_info_writer;

  minidump_file_writer.AddStream(&misc_info_writer);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&file_writer));

  const MINIDUMP_MISC_INFO* observed;
  ASSERT_NO_FATAL_FAILURE(GetMiscInfoStream(file_writer.string(), &observed));

  MINIDUMP_MISC_INFO expected = {};

  ExpectMiscInfoEqual(&expected, observed);
}

TEST(MinidumpMiscInfoWriter, ProcessId) {
  MinidumpFileWriter minidump_file_writer;
  MinidumpMiscInfoWriter misc_info_writer;

  const uint32_t kProcessId = 12345;

  misc_info_writer.SetProcessId(kProcessId);

  minidump_file_writer.AddStream(&misc_info_writer);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&file_writer));

  const MINIDUMP_MISC_INFO* observed;
  ASSERT_NO_FATAL_FAILURE(GetMiscInfoStream(file_writer.string(), &observed));

  MINIDUMP_MISC_INFO expected = {};
  expected.Flags1 = MINIDUMP_MISC1_PROCESS_ID;
  expected.ProcessId = kProcessId;

  ExpectMiscInfoEqual(&expected, observed);
}

TEST(MinidumpMiscInfoWriter, ProcessTimes) {
  MinidumpFileWriter minidump_file_writer;
  MinidumpMiscInfoWriter misc_info_writer;

  const time_t kProcessCreateTime = 0x15252f00;
  const uint32_t kProcessUserTime = 10;
  const uint32_t kProcessKernelTime = 5;

  misc_info_writer.SetProcessTimes(
      kProcessCreateTime, kProcessUserTime, kProcessKernelTime);

  minidump_file_writer.AddStream(&misc_info_writer);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&file_writer));

  const MINIDUMP_MISC_INFO* observed;
  ASSERT_NO_FATAL_FAILURE(GetMiscInfoStream(file_writer.string(), &observed));

  MINIDUMP_MISC_INFO expected = {};
  expected.Flags1 = MINIDUMP_MISC1_PROCESS_TIMES;
  expected.ProcessCreateTime = kProcessCreateTime;
  expected.ProcessUserTime = kProcessUserTime;
  expected.ProcessKernelTime = kProcessKernelTime;

  ExpectMiscInfoEqual(&expected, observed);
}

TEST(MinidumpMiscInfoWriter, ProcessorPowerInfo) {
  MinidumpFileWriter minidump_file_writer;
  MinidumpMiscInfoWriter misc_info_writer;

  const uint32_t kProcessorMaxMhz = 2800;
  const uint32_t kProcessorCurrentMhz = 2300;
  const uint32_t kProcessorMhzLimit = 3300;
  const uint32_t kProcessorMaxIdleState = 5;
  const uint32_t kProcessorCurrentIdleState = 1;

  misc_info_writer.SetProcessorPowerInfo(kProcessorMaxMhz,
                                         kProcessorCurrentMhz,
                                         kProcessorMhzLimit,
                                         kProcessorMaxIdleState,
                                         kProcessorCurrentIdleState);

  minidump_file_writer.AddStream(&misc_info_writer);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&file_writer));

  const MINIDUMP_MISC_INFO_2* observed;
  ASSERT_NO_FATAL_FAILURE(GetMiscInfoStream(file_writer.string(), &observed));

  MINIDUMP_MISC_INFO_2 expected = {};
  expected.Flags1 = MINIDUMP_MISC1_PROCESSOR_POWER_INFO;
  expected.ProcessorMaxMhz = kProcessorMaxMhz;
  expected.ProcessorCurrentMhz = kProcessorCurrentMhz;
  expected.ProcessorMhzLimit = kProcessorMhzLimit;
  expected.ProcessorMaxIdleState = kProcessorMaxIdleState;
  expected.ProcessorCurrentIdleState = kProcessorCurrentIdleState;

  ExpectMiscInfoEqual(&expected, observed);
}

TEST(MinidumpMiscInfoWriter, ProcessIntegrityLevel) {
  MinidumpFileWriter minidump_file_writer;
  MinidumpMiscInfoWriter misc_info_writer;

  const uint32_t kProcessIntegrityLevel = 0x2000;

  misc_info_writer.SetProcessIntegrityLevel(kProcessIntegrityLevel);

  minidump_file_writer.AddStream(&misc_info_writer);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&file_writer));

  const MINIDUMP_MISC_INFO_3* observed;
  ASSERT_NO_FATAL_FAILURE(GetMiscInfoStream(file_writer.string(), &observed));

  MINIDUMP_MISC_INFO_3 expected = {};
  expected.Flags1 = MINIDUMP_MISC3_PROCESS_INTEGRITY;
  expected.ProcessIntegrityLevel = kProcessIntegrityLevel;

  ExpectMiscInfoEqual(&expected, observed);
}

TEST(MinidumpMiscInfoWriter, ProcessExecuteFlags) {
  MinidumpFileWriter minidump_file_writer;
  MinidumpMiscInfoWriter misc_info_writer;

  const uint32_t kProcessExecuteFlags = 0x13579bdf;

  misc_info_writer.SetProcessExecuteFlags(kProcessExecuteFlags);

  minidump_file_writer.AddStream(&misc_info_writer);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&file_writer));

  const MINIDUMP_MISC_INFO_3* observed;
  ASSERT_NO_FATAL_FAILURE(GetMiscInfoStream(file_writer.string(), &observed));

  MINIDUMP_MISC_INFO_3 expected = {};
  expected.Flags1 = MINIDUMP_MISC3_PROCESS_EXECUTE_FLAGS;
  expected.ProcessExecuteFlags = kProcessExecuteFlags;

  ExpectMiscInfoEqual(&expected, observed);
}

TEST(MinidumpMiscInfoWriter, ProtectedProcess) {
  MinidumpFileWriter minidump_file_writer;
  MinidumpMiscInfoWriter misc_info_writer;

  const uint32_t kProtectedProcess = 1;

  misc_info_writer.SetProtectedProcess(kProtectedProcess);

  minidump_file_writer.AddStream(&misc_info_writer);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&file_writer));

  const MINIDUMP_MISC_INFO_3* observed;
  ASSERT_NO_FATAL_FAILURE(GetMiscInfoStream(file_writer.string(), &observed));

  MINIDUMP_MISC_INFO_3 expected = {};
  expected.Flags1 = MINIDUMP_MISC3_PROTECTED_PROCESS;
  expected.ProtectedProcess = kProtectedProcess;

  ExpectMiscInfoEqual(&expected, observed);
}

TEST(MinidumpMiscInfoWriter, TimeZone) {
  MinidumpFileWriter minidump_file_writer;
  MinidumpMiscInfoWriter misc_info_writer;

  const uint32_t kTimeZoneId = 2;
  const int32_t kBias = 300;
  const char kStandardName[] = "EST";
  const SYSTEMTIME kStandardDate = {0, 11, 1, 0, 2, 0, 0, 0};
  const int32_t kStandardBias = 0;
  const char kDaylightName[] = "EDT";
  const SYSTEMTIME kDaylightDate = {0, 3, 2, 0, 2, 0, 0, 0};
  const int32_t kDaylightBias = -60;

  misc_info_writer.SetTimeZone(kTimeZoneId,
                               kBias,
                               kStandardName,
                               kStandardDate,
                               kStandardBias,
                               kDaylightName,
                               kDaylightDate,
                               kDaylightBias);

  minidump_file_writer.AddStream(&misc_info_writer);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&file_writer));

  const MINIDUMP_MISC_INFO_3* observed;
  ASSERT_NO_FATAL_FAILURE(GetMiscInfoStream(file_writer.string(), &observed));

  MINIDUMP_MISC_INFO_3 expected = {};
  expected.Flags1 = MINIDUMP_MISC3_TIMEZONE;
  expected.TimeZoneId = kTimeZoneId;
  expected.TimeZone.Bias = kBias;
  string16 standard_name_utf16 = base::UTF8ToUTF16(kStandardName);
  c16lcpy(expected.TimeZone.StandardName,
          standard_name_utf16.c_str(),
          arraysize(expected.TimeZone.StandardName));
  memcpy(&expected.TimeZone.StandardDate,
         &kStandardDate,
         sizeof(expected.TimeZone.StandardDate));
  expected.TimeZone.StandardBias = kStandardBias;
  string16 daylight_name_utf16 = base::UTF8ToUTF16(kDaylightName);
  c16lcpy(expected.TimeZone.DaylightName,
          daylight_name_utf16.c_str(),
          arraysize(expected.TimeZone.DaylightName));
  memcpy(&expected.TimeZone.DaylightDate,
         &kDaylightDate,
         sizeof(expected.TimeZone.DaylightDate));
  expected.TimeZone.DaylightBias = kDaylightBias;

  ExpectMiscInfoEqual(&expected, observed);
}

TEST(MinidumpMiscInfoWriter, TimeZoneStringsOverflow) {
  // This test makes sure that the time zone name strings are truncated properly
  // to the widths of their fields.

  MinidumpFileWriter minidump_file_writer;
  MinidumpMiscInfoWriter misc_info_writer;

  const uint32_t kTimeZoneId = 2;
  const int32_t kBias = 300;
  std::string standard_name(
      arraysize(decltype(MINIDUMP_MISC_INFO_N::TimeZone)::StandardName) + 1,
      's');
  const int32_t kStandardBias = 0;
  std::string daylight_name(
      arraysize(decltype(MINIDUMP_MISC_INFO_N::TimeZone)::DaylightName), 'd');
  const int32_t kDaylightBias = -60;

  // Test using kSystemTimeZero, because not all platforms will be able to
  // provide daylight saving time transition times.
  const SYSTEMTIME kSystemTimeZero = {};

  misc_info_writer.SetTimeZone(kTimeZoneId,
                               kBias,
                               standard_name,
                               kSystemTimeZero,
                               kStandardBias,
                               daylight_name,
                               kSystemTimeZero,
                               kDaylightBias);

  minidump_file_writer.AddStream(&misc_info_writer);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&file_writer));

  const MINIDUMP_MISC_INFO_3* observed;
  ASSERT_NO_FATAL_FAILURE(GetMiscInfoStream(file_writer.string(), &observed));

  MINIDUMP_MISC_INFO_3 expected = {};
  expected.Flags1 = MINIDUMP_MISC3_TIMEZONE;
  expected.TimeZoneId = kTimeZoneId;
  expected.TimeZone.Bias = kBias;
  string16 standard_name_utf16 = base::UTF8ToUTF16(standard_name);
  c16lcpy(expected.TimeZone.StandardName,
          standard_name_utf16.c_str(),
          arraysize(expected.TimeZone.StandardName));
  memcpy(&expected.TimeZone.StandardDate,
         &kSystemTimeZero,
         sizeof(expected.TimeZone.StandardDate));
  expected.TimeZone.StandardBias = kStandardBias;
  string16 daylight_name_utf16 = base::UTF8ToUTF16(daylight_name);
  c16lcpy(expected.TimeZone.DaylightName,
          daylight_name_utf16.c_str(),
          arraysize(expected.TimeZone.DaylightName));
  memcpy(&expected.TimeZone.DaylightDate,
         &kSystemTimeZero,
         sizeof(expected.TimeZone.DaylightDate));
  expected.TimeZone.DaylightBias = kDaylightBias;

  ExpectMiscInfoEqual(&expected, observed);
}

TEST(MinidumpMiscInfoWriter, BuildStrings) {
  MinidumpFileWriter minidump_file_writer;
  MinidumpMiscInfoWriter misc_info_writer;

  const char kBuildString[] = "build string";
  const char kDebugBuildString[] = "debug build string";

  misc_info_writer.SetBuildString(kBuildString, kDebugBuildString);

  minidump_file_writer.AddStream(&misc_info_writer);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&file_writer));

  const MINIDUMP_MISC_INFO_4* observed;
  ASSERT_NO_FATAL_FAILURE(GetMiscInfoStream(file_writer.string(), &observed));

  MINIDUMP_MISC_INFO_4 expected = {};
  expected.Flags1 = MINIDUMP_MISC4_BUILDSTRING;
  string16 build_string_utf16 = base::UTF8ToUTF16(kBuildString);
  c16lcpy(expected.BuildString,
          build_string_utf16.c_str(),
          arraysize(expected.BuildString));
  string16 debug_build_string_utf16 = base::UTF8ToUTF16(kDebugBuildString);
  c16lcpy(expected.DbgBldStr,
          debug_build_string_utf16.c_str(),
          arraysize(expected.DbgBldStr));

  ExpectMiscInfoEqual(&expected, observed);
}

TEST(MinidumpMiscInfoWriter, BuildStringsOverflow) {
  // This test makes sure that the build strings are truncated properly to the
  // widths of their fields.

  MinidumpFileWriter minidump_file_writer;
  MinidumpMiscInfoWriter misc_info_writer;

  std::string build_string(arraysize(MINIDUMP_MISC_INFO_N::BuildString) + 1,
                           'B');
  std::string debug_build_string(arraysize(MINIDUMP_MISC_INFO_N::DbgBldStr),
                                 'D');

  misc_info_writer.SetBuildString(build_string, debug_build_string);

  minidump_file_writer.AddStream(&misc_info_writer);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&file_writer));

  const MINIDUMP_MISC_INFO_4* observed;
  ASSERT_NO_FATAL_FAILURE(GetMiscInfoStream(file_writer.string(), &observed));

  MINIDUMP_MISC_INFO_4 expected = {};
  expected.Flags1 = MINIDUMP_MISC4_BUILDSTRING;
  string16 build_string_utf16 = base::UTF8ToUTF16(build_string);
  c16lcpy(expected.BuildString,
          build_string_utf16.c_str(),
          arraysize(expected.BuildString));
  string16 debug_build_string_utf16 = base::UTF8ToUTF16(debug_build_string);
  c16lcpy(expected.DbgBldStr,
          debug_build_string_utf16.c_str(),
          arraysize(expected.DbgBldStr));

  ExpectMiscInfoEqual(&expected, observed);
}

TEST(MinidumpMiscInfoWriter, Everything) {
  MinidumpFileWriter minidump_file_writer;
  MinidumpMiscInfoWriter misc_info_writer;

  const uint32_t kProcessId = 12345;
  const time_t kProcessCreateTime = 0x15252f00;
  const uint32_t kProcessUserTime = 10;
  const uint32_t kProcessKernelTime = 5;
  const uint32_t kProcessorMaxMhz = 2800;
  const uint32_t kProcessorCurrentMhz = 2300;
  const uint32_t kProcessorMhzLimit = 3300;
  const uint32_t kProcessorMaxIdleState = 5;
  const uint32_t kProcessorCurrentIdleState = 1;
  const uint32_t kProcessIntegrityLevel = 0x2000;
  const uint32_t kProcessExecuteFlags = 0x13579bdf;
  const uint32_t kProtectedProcess = 1;
  const uint32_t kTimeZoneId = 2;
  const int32_t kBias = 300;
  const char kStandardName[] = "EST";
  const int32_t kStandardBias = 0;
  const char kDaylightName[] = "EDT";
  const int32_t kDaylightBias = -60;
  const SYSTEMTIME kSystemTimeZero = {};
  const char kBuildString[] = "build string";
  const char kDebugBuildString[] = "debug build string";

  misc_info_writer.SetProcessId(kProcessId);
  misc_info_writer.SetProcessTimes(
      kProcessCreateTime, kProcessUserTime, kProcessKernelTime);
  misc_info_writer.SetProcessorPowerInfo(kProcessorMaxMhz,
                                         kProcessorCurrentMhz,
                                         kProcessorMhzLimit,
                                         kProcessorMaxIdleState,
                                         kProcessorCurrentIdleState);
  misc_info_writer.SetProcessIntegrityLevel(kProcessIntegrityLevel);
  misc_info_writer.SetProcessExecuteFlags(kProcessExecuteFlags);
  misc_info_writer.SetProtectedProcess(kProtectedProcess);
  misc_info_writer.SetTimeZone(kTimeZoneId,
                               kBias,
                               kStandardName,
                               kSystemTimeZero,
                               kStandardBias,
                               kDaylightName,
                               kSystemTimeZero,
                               kDaylightBias);
  misc_info_writer.SetBuildString(kBuildString, kDebugBuildString);

  minidump_file_writer.AddStream(&misc_info_writer);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&file_writer));

  const MINIDUMP_MISC_INFO_4* observed;
  ASSERT_NO_FATAL_FAILURE(GetMiscInfoStream(file_writer.string(), &observed));

  MINIDUMP_MISC_INFO_4 expected = {};
  expected.Flags1 =
      MINIDUMP_MISC1_PROCESS_ID | MINIDUMP_MISC1_PROCESS_TIMES |
      MINIDUMP_MISC1_PROCESSOR_POWER_INFO | MINIDUMP_MISC3_PROCESS_INTEGRITY |
      MINIDUMP_MISC3_PROCESS_EXECUTE_FLAGS | MINIDUMP_MISC3_PROTECTED_PROCESS |
      MINIDUMP_MISC3_TIMEZONE | MINIDUMP_MISC4_BUILDSTRING;
  expected.ProcessId = kProcessId;
  expected.ProcessCreateTime = kProcessCreateTime;
  expected.ProcessUserTime = kProcessUserTime;
  expected.ProcessKernelTime = kProcessKernelTime;
  expected.ProcessorMaxMhz = kProcessorMaxMhz;
  expected.ProcessorCurrentMhz = kProcessorCurrentMhz;
  expected.ProcessorMhzLimit = kProcessorMhzLimit;
  expected.ProcessorMaxIdleState = kProcessorMaxIdleState;
  expected.ProcessorCurrentIdleState = kProcessorCurrentIdleState;
  expected.ProcessIntegrityLevel = kProcessIntegrityLevel;
  expected.ProcessExecuteFlags = kProcessExecuteFlags;
  expected.ProtectedProcess = kProtectedProcess;
  expected.TimeZoneId = kTimeZoneId;
  expected.TimeZone.Bias = kBias;
  string16 standard_name_utf16 = base::UTF8ToUTF16(kStandardName);
  c16lcpy(expected.TimeZone.StandardName,
          standard_name_utf16.c_str(),
          arraysize(expected.TimeZone.StandardName));
  memcpy(&expected.TimeZone.StandardDate,
         &kSystemTimeZero,
         sizeof(expected.TimeZone.StandardDate));
  expected.TimeZone.StandardBias = kStandardBias;
  string16 daylight_name_utf16 = base::UTF8ToUTF16(kDaylightName);
  c16lcpy(expected.TimeZone.DaylightName,
          daylight_name_utf16.c_str(),
          arraysize(expected.TimeZone.DaylightName));
  memcpy(&expected.TimeZone.DaylightDate,
         &kSystemTimeZero,
         sizeof(expected.TimeZone.DaylightDate));
  expected.TimeZone.DaylightBias = kDaylightBias;
  string16 build_string_utf16 = base::UTF8ToUTF16(kBuildString);
  c16lcpy(expected.BuildString,
          build_string_utf16.c_str(),
          arraysize(expected.BuildString));
  string16 debug_build_string_utf16 = base::UTF8ToUTF16(kDebugBuildString);
  c16lcpy(expected.DbgBldStr,
          debug_build_string_utf16.c_str(),
          arraysize(expected.DbgBldStr));

  ExpectMiscInfoEqual(&expected, observed);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
