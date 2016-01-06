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

#include <string.h>

#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "gtest/gtest.h"
#include "minidump/minidump_file_writer.h"
#include "minidump/test/minidump_file_writer_test_util.h"
#include "minidump/test/minidump_writable_test_util.h"
#include "snapshot/test/test_process_snapshot.h"
#include "snapshot/test/test_system_snapshot.h"
#include "util/file/string_file.h"
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

void ExpectNULPaddedString16Equal(const base::char16* expected,
                                  const base::char16* observed,
                                  size_t size) {
  base::string16 expected_string(expected, size);
  base::string16 observed_string(observed, size);
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
  ExpectMiscInfoEqual<MINIDUMP_MISC_INFO>(
      reinterpret_cast<const MINIDUMP_MISC_INFO*>(expected),
      reinterpret_cast<const MINIDUMP_MISC_INFO*>(observed));
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
  ExpectMiscInfoEqual<MINIDUMP_MISC_INFO_2>(
      reinterpret_cast<const MINIDUMP_MISC_INFO_2*>(expected),
      reinterpret_cast<const MINIDUMP_MISC_INFO_2*>(observed));
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
  ExpectMiscInfoEqual<MINIDUMP_MISC_INFO_3>(
      reinterpret_cast<const MINIDUMP_MISC_INFO_3*>(expected),
      reinterpret_cast<const MINIDUMP_MISC_INFO_3*>(observed));
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
  auto misc_info_writer = make_scoped_ptr(new MinidumpMiscInfoWriter());

  minidump_file_writer.AddStream(std::move(misc_info_writer));

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  const MINIDUMP_MISC_INFO* observed = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetMiscInfoStream(string_file.string(), &observed));

  MINIDUMP_MISC_INFO expected = {};

  ExpectMiscInfoEqual(&expected, observed);
}

TEST(MinidumpMiscInfoWriter, ProcessId) {
  MinidumpFileWriter minidump_file_writer;
  auto misc_info_writer = make_scoped_ptr(new MinidumpMiscInfoWriter());

  const uint32_t kProcessId = 12345;

  misc_info_writer->SetProcessID(kProcessId);

  minidump_file_writer.AddStream(std::move(misc_info_writer));

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  const MINIDUMP_MISC_INFO* observed = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetMiscInfoStream(string_file.string(), &observed));

  MINIDUMP_MISC_INFO expected = {};
  expected.Flags1 = MINIDUMP_MISC1_PROCESS_ID;
  expected.ProcessId = kProcessId;

  ExpectMiscInfoEqual(&expected, observed);
}

TEST(MinidumpMiscInfoWriter, ProcessTimes) {
  MinidumpFileWriter minidump_file_writer;
  auto misc_info_writer = make_scoped_ptr(new MinidumpMiscInfoWriter());

  const time_t kProcessCreateTime = 0x15252f00;
  const uint32_t kProcessUserTime = 10;
  const uint32_t kProcessKernelTime = 5;

  misc_info_writer->SetProcessTimes(
      kProcessCreateTime, kProcessUserTime, kProcessKernelTime);

  minidump_file_writer.AddStream(std::move(misc_info_writer));

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  const MINIDUMP_MISC_INFO* observed = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetMiscInfoStream(string_file.string(), &observed));

  MINIDUMP_MISC_INFO expected = {};
  expected.Flags1 = MINIDUMP_MISC1_PROCESS_TIMES;
  expected.ProcessCreateTime = kProcessCreateTime;
  expected.ProcessUserTime = kProcessUserTime;
  expected.ProcessKernelTime = kProcessKernelTime;

  ExpectMiscInfoEqual(&expected, observed);
}

TEST(MinidumpMiscInfoWriter, ProcessorPowerInfo) {
  MinidumpFileWriter minidump_file_writer;
  auto misc_info_writer = make_scoped_ptr(new MinidumpMiscInfoWriter());

  const uint32_t kProcessorMaxMhz = 2800;
  const uint32_t kProcessorCurrentMhz = 2300;
  const uint32_t kProcessorMhzLimit = 3300;
  const uint32_t kProcessorMaxIdleState = 5;
  const uint32_t kProcessorCurrentIdleState = 1;

  misc_info_writer->SetProcessorPowerInfo(kProcessorMaxMhz,
                                          kProcessorCurrentMhz,
                                          kProcessorMhzLimit,
                                          kProcessorMaxIdleState,
                                          kProcessorCurrentIdleState);

  minidump_file_writer.AddStream(std::move(misc_info_writer));

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  const MINIDUMP_MISC_INFO_2* observed = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetMiscInfoStream(string_file.string(), &observed));

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
  auto misc_info_writer = make_scoped_ptr(new MinidumpMiscInfoWriter());

  const uint32_t kProcessIntegrityLevel = 0x2000;

  misc_info_writer->SetProcessIntegrityLevel(kProcessIntegrityLevel);

  minidump_file_writer.AddStream(std::move(misc_info_writer));

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  const MINIDUMP_MISC_INFO_3* observed = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetMiscInfoStream(string_file.string(), &observed));

  MINIDUMP_MISC_INFO_3 expected = {};
  expected.Flags1 = MINIDUMP_MISC3_PROCESS_INTEGRITY;
  expected.ProcessIntegrityLevel = kProcessIntegrityLevel;

  ExpectMiscInfoEqual(&expected, observed);
}

TEST(MinidumpMiscInfoWriter, ProcessExecuteFlags) {
  MinidumpFileWriter minidump_file_writer;
  auto misc_info_writer = make_scoped_ptr(new MinidumpMiscInfoWriter());

  const uint32_t kProcessExecuteFlags = 0x13579bdf;

  misc_info_writer->SetProcessExecuteFlags(kProcessExecuteFlags);

  minidump_file_writer.AddStream(std::move(misc_info_writer));

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  const MINIDUMP_MISC_INFO_3* observed = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetMiscInfoStream(string_file.string(), &observed));

  MINIDUMP_MISC_INFO_3 expected = {};
  expected.Flags1 = MINIDUMP_MISC3_PROCESS_EXECUTE_FLAGS;
  expected.ProcessExecuteFlags = kProcessExecuteFlags;

  ExpectMiscInfoEqual(&expected, observed);
}

TEST(MinidumpMiscInfoWriter, ProtectedProcess) {
  MinidumpFileWriter minidump_file_writer;
  auto misc_info_writer = make_scoped_ptr(new MinidumpMiscInfoWriter());

  const uint32_t kProtectedProcess = 1;

  misc_info_writer->SetProtectedProcess(kProtectedProcess);

  minidump_file_writer.AddStream(std::move(misc_info_writer));

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  const MINIDUMP_MISC_INFO_3* observed = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetMiscInfoStream(string_file.string(), &observed));

  MINIDUMP_MISC_INFO_3 expected = {};
  expected.Flags1 = MINIDUMP_MISC3_PROTECTED_PROCESS;
  expected.ProtectedProcess = kProtectedProcess;

  ExpectMiscInfoEqual(&expected, observed);
}

TEST(MinidumpMiscInfoWriter, TimeZone) {
  MinidumpFileWriter minidump_file_writer;
  auto misc_info_writer = make_scoped_ptr(new MinidumpMiscInfoWriter());

  const uint32_t kTimeZoneId = 2;
  const int32_t kBias = 300;
  const char kStandardName[] = "EST";
  const SYSTEMTIME kStandardDate = {0, 11, 1, 0, 2, 0, 0, 0};
  const int32_t kStandardBias = 0;
  const char kDaylightName[] = "EDT";
  const SYSTEMTIME kDaylightDate = {0, 3, 2, 0, 2, 0, 0, 0};
  const int32_t kDaylightBias = -60;

  misc_info_writer->SetTimeZone(kTimeZoneId,
                                kBias,
                                kStandardName,
                                kStandardDate,
                                kStandardBias,
                                kDaylightName,
                                kDaylightDate,
                                kDaylightBias);

  minidump_file_writer.AddStream(std::move(misc_info_writer));

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  const MINIDUMP_MISC_INFO_3* observed = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetMiscInfoStream(string_file.string(), &observed));

  MINIDUMP_MISC_INFO_3 expected = {};
  expected.Flags1 = MINIDUMP_MISC3_TIMEZONE;
  expected.TimeZoneId = kTimeZoneId;
  expected.TimeZone.Bias = kBias;
  base::string16 standard_name_utf16 = base::UTF8ToUTF16(kStandardName);
  c16lcpy(expected.TimeZone.StandardName,
          standard_name_utf16.c_str(),
          arraysize(expected.TimeZone.StandardName));
  memcpy(&expected.TimeZone.StandardDate,
         &kStandardDate,
         sizeof(expected.TimeZone.StandardDate));
  expected.TimeZone.StandardBias = kStandardBias;
  base::string16 daylight_name_utf16 = base::UTF8ToUTF16(kDaylightName);
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
  auto misc_info_writer = make_scoped_ptr(new MinidumpMiscInfoWriter());

  const uint32_t kTimeZoneId = 2;
  const int32_t kBias = 300;
  MINIDUMP_MISC_INFO_N tmp;
  ALLOW_UNUSED_LOCAL(tmp);
  std::string standard_name(arraysize(tmp.TimeZone.StandardName) + 1, 's');
  const int32_t kStandardBias = 0;
  std::string daylight_name(arraysize(tmp.TimeZone.DaylightName), 'd');
  const int32_t kDaylightBias = -60;

  // Test using kSystemTimeZero, because not all platforms will be able to
  // provide daylight saving time transition times.
  const SYSTEMTIME kSystemTimeZero = {};

  misc_info_writer->SetTimeZone(kTimeZoneId,
                                kBias,
                                standard_name,
                                kSystemTimeZero,
                                kStandardBias,
                                daylight_name,
                                kSystemTimeZero,
                                kDaylightBias);

  minidump_file_writer.AddStream(std::move(misc_info_writer));

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  const MINIDUMP_MISC_INFO_3* observed = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetMiscInfoStream(string_file.string(), &observed));

  MINIDUMP_MISC_INFO_3 expected = {};
  expected.Flags1 = MINIDUMP_MISC3_TIMEZONE;
  expected.TimeZoneId = kTimeZoneId;
  expected.TimeZone.Bias = kBias;
  base::string16 standard_name_utf16 = base::UTF8ToUTF16(standard_name);
  c16lcpy(expected.TimeZone.StandardName,
          standard_name_utf16.c_str(),
          arraysize(expected.TimeZone.StandardName));
  memcpy(&expected.TimeZone.StandardDate,
         &kSystemTimeZero,
         sizeof(expected.TimeZone.StandardDate));
  expected.TimeZone.StandardBias = kStandardBias;
  base::string16 daylight_name_utf16 = base::UTF8ToUTF16(daylight_name);
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
  auto misc_info_writer = make_scoped_ptr(new MinidumpMiscInfoWriter());

  const char kBuildString[] = "build string";
  const char kDebugBuildString[] = "debug build string";

  misc_info_writer->SetBuildString(kBuildString, kDebugBuildString);

  minidump_file_writer.AddStream(std::move(misc_info_writer));

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  const MINIDUMP_MISC_INFO_4* observed = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetMiscInfoStream(string_file.string(), &observed));

  MINIDUMP_MISC_INFO_4 expected = {};
  expected.Flags1 = MINIDUMP_MISC4_BUILDSTRING;
  base::string16 build_string_utf16 = base::UTF8ToUTF16(kBuildString);
  c16lcpy(expected.BuildString,
          build_string_utf16.c_str(),
          arraysize(expected.BuildString));
  base::string16 debug_build_string_utf16 =
      base::UTF8ToUTF16(kDebugBuildString);
  c16lcpy(expected.DbgBldStr,
          debug_build_string_utf16.c_str(),
          arraysize(expected.DbgBldStr));

  ExpectMiscInfoEqual(&expected, observed);
}

TEST(MinidumpMiscInfoWriter, BuildStringsOverflow) {
  // This test makes sure that the build strings are truncated properly to the
  // widths of their fields.

  MinidumpFileWriter minidump_file_writer;
  auto misc_info_writer = make_scoped_ptr(new MinidumpMiscInfoWriter());

  MINIDUMP_MISC_INFO_4 tmp;
  ALLOW_UNUSED_LOCAL(tmp);
  std::string build_string(arraysize(tmp.BuildString) + 1, 'B');
  std::string debug_build_string(arraysize(tmp.DbgBldStr), 'D');

  misc_info_writer->SetBuildString(build_string, debug_build_string);

  minidump_file_writer.AddStream(std::move(misc_info_writer));

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  const MINIDUMP_MISC_INFO_4* observed = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetMiscInfoStream(string_file.string(), &observed));

  MINIDUMP_MISC_INFO_4 expected = {};
  expected.Flags1 = MINIDUMP_MISC4_BUILDSTRING;
  base::string16 build_string_utf16 = base::UTF8ToUTF16(build_string);
  c16lcpy(expected.BuildString,
          build_string_utf16.c_str(),
          arraysize(expected.BuildString));
  base::string16 debug_build_string_utf16 =
      base::UTF8ToUTF16(debug_build_string);
  c16lcpy(expected.DbgBldStr,
          debug_build_string_utf16.c_str(),
          arraysize(expected.DbgBldStr));

  ExpectMiscInfoEqual(&expected, observed);
}

TEST(MinidumpMiscInfoWriter, Everything) {
  MinidumpFileWriter minidump_file_writer;
  auto misc_info_writer = make_scoped_ptr(new MinidumpMiscInfoWriter());

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

  misc_info_writer->SetProcessID(kProcessId);
  misc_info_writer->SetProcessTimes(
      kProcessCreateTime, kProcessUserTime, kProcessKernelTime);
  misc_info_writer->SetProcessorPowerInfo(kProcessorMaxMhz,
                                          kProcessorCurrentMhz,
                                          kProcessorMhzLimit,
                                          kProcessorMaxIdleState,
                                          kProcessorCurrentIdleState);
  misc_info_writer->SetProcessIntegrityLevel(kProcessIntegrityLevel);
  misc_info_writer->SetProcessExecuteFlags(kProcessExecuteFlags);
  misc_info_writer->SetProtectedProcess(kProtectedProcess);
  misc_info_writer->SetTimeZone(kTimeZoneId,
                                kBias,
                                kStandardName,
                                kSystemTimeZero,
                                kStandardBias,
                                kDaylightName,
                                kSystemTimeZero,
                                kDaylightBias);
  misc_info_writer->SetBuildString(kBuildString, kDebugBuildString);

  minidump_file_writer.AddStream(std::move(misc_info_writer));

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  const MINIDUMP_MISC_INFO_4* observed = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetMiscInfoStream(string_file.string(), &observed));

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
  base::string16 standard_name_utf16 = base::UTF8ToUTF16(kStandardName);
  c16lcpy(expected.TimeZone.StandardName,
          standard_name_utf16.c_str(),
          arraysize(expected.TimeZone.StandardName));
  memcpy(&expected.TimeZone.StandardDate,
         &kSystemTimeZero,
         sizeof(expected.TimeZone.StandardDate));
  expected.TimeZone.StandardBias = kStandardBias;
  base::string16 daylight_name_utf16 = base::UTF8ToUTF16(kDaylightName);
  c16lcpy(expected.TimeZone.DaylightName,
          daylight_name_utf16.c_str(),
          arraysize(expected.TimeZone.DaylightName));
  memcpy(&expected.TimeZone.DaylightDate,
         &kSystemTimeZero,
         sizeof(expected.TimeZone.DaylightDate));
  expected.TimeZone.DaylightBias = kDaylightBias;
  base::string16 build_string_utf16 = base::UTF8ToUTF16(kBuildString);
  c16lcpy(expected.BuildString,
          build_string_utf16.c_str(),
          arraysize(expected.BuildString));
  base::string16 debug_build_string_utf16 =
      base::UTF8ToUTF16(kDebugBuildString);
  c16lcpy(expected.DbgBldStr,
          debug_build_string_utf16.c_str(),
          arraysize(expected.DbgBldStr));

  ExpectMiscInfoEqual(&expected, observed);
}

TEST(MinidumpMiscInfoWriter, InitializeFromSnapshot) {
  MINIDUMP_MISC_INFO_4 expect_misc_info = {};

  const char kStandardTimeName[] = "EST";
  const char kDaylightTimeName[] = "EDT";
  const char kOSVersionFull[] =
      "Mac OS X 10.9.5 (13F34); "
      "Darwin 13.4.0 Darwin Kernel Version 13.4.0: "
      "Sun Aug 17 19:50:11 PDT 2014; "
      "root:xnu-2422.115.4~1/RELEASE_X86_64 x86_64";
  const char kMachineDescription[] = "MacBookPro11,3 (Mac-2BD1B31983FE1663)";
  base::string16 standard_time_name_utf16 =
      base::UTF8ToUTF16(kStandardTimeName);
  base::string16 daylight_time_name_utf16 =
      base::UTF8ToUTF16(kDaylightTimeName);
  base::string16 build_string_utf16 = base::UTF8ToUTF16(
      std::string(kOSVersionFull) + "; " + kMachineDescription);
  std::string debug_build_string = internal::MinidumpMiscInfoDebugBuildString();
  EXPECT_FALSE(debug_build_string.empty());
  base::string16 debug_build_string_utf16 =
      base::UTF8ToUTF16(debug_build_string);

  expect_misc_info.SizeOfInfo = sizeof(expect_misc_info);
  expect_misc_info.Flags1 = MINIDUMP_MISC1_PROCESS_ID |
                            MINIDUMP_MISC1_PROCESS_TIMES |
                            MINIDUMP_MISC1_PROCESSOR_POWER_INFO |
                            MINIDUMP_MISC3_TIMEZONE |
                            MINIDUMP_MISC4_BUILDSTRING;
  expect_misc_info.ProcessId = 12345;
  expect_misc_info.ProcessCreateTime = 0x555c7740;
  expect_misc_info.ProcessUserTime = 60;
  expect_misc_info.ProcessKernelTime = 15;
  expect_misc_info.ProcessorCurrentMhz = 2800;
  expect_misc_info.ProcessorMaxMhz = 2800;
  expect_misc_info.TimeZoneId = 1;
  expect_misc_info.TimeZone.Bias = 300;
  c16lcpy(expect_misc_info.TimeZone.StandardName,
          standard_time_name_utf16.c_str(),
          arraysize(expect_misc_info.TimeZone.StandardName));
  expect_misc_info.TimeZone.StandardBias = 0;
  c16lcpy(expect_misc_info.TimeZone.DaylightName,
          daylight_time_name_utf16.c_str(),
          arraysize(expect_misc_info.TimeZone.DaylightName));
  expect_misc_info.TimeZone.DaylightBias = -60;
  c16lcpy(expect_misc_info.BuildString,
          build_string_utf16.c_str(),
          arraysize(expect_misc_info.BuildString));
  c16lcpy(expect_misc_info.DbgBldStr,
          debug_build_string_utf16.c_str(),
          arraysize(expect_misc_info.DbgBldStr));

  const timeval kStartTime =
      { static_cast<time_t>(expect_misc_info.ProcessCreateTime), 0 };
  const timeval kUserCPUTime =
      { static_cast<time_t>(expect_misc_info.ProcessUserTime), 0 };
  const timeval kSystemCPUTime =
      { static_cast<time_t>(expect_misc_info.ProcessKernelTime), 0 };

  TestProcessSnapshot process_snapshot;
  process_snapshot.SetProcessID(expect_misc_info.ProcessId);
  process_snapshot.SetProcessStartTime(kStartTime);
  process_snapshot.SetProcessCPUTimes(kUserCPUTime, kSystemCPUTime);

  auto system_snapshot = make_scoped_ptr(new TestSystemSnapshot());
  const uint64_t kHzPerMHz = static_cast<uint64_t>(1E6);
  system_snapshot->SetCPUFrequency(
      expect_misc_info.ProcessorCurrentMhz * kHzPerMHz,
      expect_misc_info.ProcessorMaxMhz * kHzPerMHz);
  system_snapshot->SetTimeZone(SystemSnapshot::kObservingStandardTime,
                               expect_misc_info.TimeZone.Bias * -60,
                               (expect_misc_info.TimeZone.Bias +
                                expect_misc_info.TimeZone.DaylightBias) * -60,
                               kStandardTimeName,
                               kDaylightTimeName);
  system_snapshot->SetOSVersionFull(kOSVersionFull);
  system_snapshot->SetMachineDescription(kMachineDescription);

  process_snapshot.SetSystem(std::move(system_snapshot));

  auto misc_info_writer = make_scoped_ptr(new MinidumpMiscInfoWriter());
  misc_info_writer->InitializeFromSnapshot(&process_snapshot);

  MinidumpFileWriter minidump_file_writer;
  minidump_file_writer.AddStream(std::move(misc_info_writer));

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  const MINIDUMP_MISC_INFO_4* misc_info = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetMiscInfoStream(string_file.string(), &misc_info));

  ExpectMiscInfoEqual(&expect_misc_info, misc_info);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
