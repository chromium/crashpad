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

#include "minidump/minidump_exception_writer.h"

#include <dbghelp.h>
#include <stdint.h>
#include <sys/types.h>

#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "minidump/minidump_context.h"
#include "minidump/minidump_context_writer.h"
#include "minidump/minidump_extensions.h"
#include "minidump/minidump_file_writer.h"
#include "minidump/test/minidump_context_test_util.h"
#include "minidump/test/minidump_file_writer_test_util.h"
#include "minidump/test/minidump_writable_test_util.h"
#include "util/file/string_file_writer.h"

namespace crashpad {
namespace test {
namespace {

// This returns the MINIDUMP_EXCEPTION_STREAM stream in |exception_stream|.
void GetExceptionStream(const std::string& file_contents,
                        const MINIDUMP_EXCEPTION_STREAM** exception_stream) {
  const size_t kDirectoryOffset = sizeof(MINIDUMP_HEADER);
  const size_t kExceptionStreamOffset =
      kDirectoryOffset + sizeof(MINIDUMP_DIRECTORY);
  const size_t kContextOffset =
      kExceptionStreamOffset + sizeof(MINIDUMP_EXCEPTION_STREAM);
  const size_t kFileSize = kContextOffset + sizeof(MinidumpContextX86);
  ASSERT_EQ(file_contents.size(), kFileSize);

  const MINIDUMP_DIRECTORY* directory;
  const MINIDUMP_HEADER* header =
      MinidumpHeaderAtStart(file_contents, &directory);
  ASSERT_NO_FATAL_FAILURE(VerifyMinidumpHeader(header, 1, 0));

  ASSERT_EQ(kMinidumpStreamTypeException, directory[0].StreamType);
  EXPECT_EQ(kExceptionStreamOffset, directory[0].Location.Rva);

  *exception_stream =
      MinidumpWritableAtLocationDescriptor<MINIDUMP_EXCEPTION_STREAM>(
          file_contents, directory[0].Location);
  ASSERT_TRUE(exception_stream);
}

// The MINIDUMP_EXCEPTION_STREAMs |expected| and |observed| are compared against
// each other using gtest assertions. The context will be recovered from
// |file_contents| and stored in |context|.
void ExpectExceptionStream(const MINIDUMP_EXCEPTION_STREAM* expected,
                           const MINIDUMP_EXCEPTION_STREAM* observed,
                           const std::string& file_contents,
                           const MinidumpContextX86** context) {
  EXPECT_EQ(expected->ThreadId, observed->ThreadId);
  EXPECT_EQ(0u, observed->__alignment);
  EXPECT_EQ(expected->ExceptionRecord.ExceptionCode,
            observed->ExceptionRecord.ExceptionCode);
  EXPECT_EQ(expected->ExceptionRecord.ExceptionFlags,
            observed->ExceptionRecord.ExceptionFlags);
  EXPECT_EQ(expected->ExceptionRecord.ExceptionRecord,
            observed->ExceptionRecord.ExceptionRecord);
  EXPECT_EQ(expected->ExceptionRecord.ExceptionAddress,
            observed->ExceptionRecord.ExceptionAddress);
  EXPECT_EQ(expected->ExceptionRecord.NumberParameters,
            observed->ExceptionRecord.NumberParameters);
  EXPECT_EQ(0u, observed->ExceptionRecord.__unusedAlignment);
  for (size_t index = 0;
       index < arraysize(observed->ExceptionRecord.ExceptionInformation);
       ++index) {
    EXPECT_EQ(expected->ExceptionRecord.ExceptionInformation[index],
              observed->ExceptionRecord.ExceptionInformation[index]);
  }
  *context = MinidumpWritableAtLocationDescriptor<MinidumpContextX86>(
      file_contents, observed->ThreadContext);
  ASSERT_TRUE(context);
}

TEST(MinidumpExceptionWriter, Minimal) {
  MinidumpFileWriter minidump_file_writer;
  MinidumpExceptionWriter exception_writer;

  const uint32_t kSeed = 100;

  MinidumpContextX86Writer context_x86_writer;
  InitializeMinidumpContextX86(context_x86_writer.context(), kSeed);
  exception_writer.SetContext(&context_x86_writer);

  minidump_file_writer.AddStream(&exception_writer);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&file_writer));

  const MINIDUMP_EXCEPTION_STREAM* observed_exception_stream;
  ASSERT_NO_FATAL_FAILURE(
      GetExceptionStream(file_writer.string(), &observed_exception_stream));

  MINIDUMP_EXCEPTION_STREAM expected_exception_stream = {};
  expected_exception_stream.ThreadContext.DataSize = sizeof(MinidumpContextX86);

  const MinidumpContextX86* observed_context;
  ASSERT_NO_FATAL_FAILURE(ExpectExceptionStream(&expected_exception_stream,
                                                observed_exception_stream,
                                                file_writer.string(),
                                                &observed_context));

  ASSERT_NO_FATAL_FAILURE(ExpectMinidumpContextX86(kSeed, observed_context));
}

TEST(MinidumpExceptionWriter, Standard) {
  MinidumpFileWriter minidump_file_writer;
  MinidumpExceptionWriter exception_writer;

  const uint32_t kSeed = 200;
  const uint32_t kThreadID = 1;
  const uint32_t kExceptionCode = 2;
  const uint32_t kExceptionFlags = 3;
  const uint32_t kExceptionRecord = 4;
  const uint32_t kExceptionAddress = 5;
  const uint64_t kExceptionInformation0 = 6;
  const uint64_t kExceptionInformation1 = 7;
  const uint64_t kExceptionInformation2 = 7;

  MinidumpContextX86Writer context_x86_writer;
  InitializeMinidumpContextX86(context_x86_writer.context(), kSeed);
  exception_writer.SetContext(&context_x86_writer);

  exception_writer.SetThreadID(kThreadID);
  exception_writer.SetExceptionCode(kExceptionCode);
  exception_writer.SetExceptionFlags(kExceptionFlags);
  exception_writer.SetExceptionRecord(kExceptionRecord);
  exception_writer.SetExceptionAddress(kExceptionAddress);

  // Set a lot of exception information at first, and then replace it with less.
  // This tests that the exception that is written does not contain the
  // “garbage” from the initial SetExceptionInformation() call.
  std::vector<uint64_t> exception_information(EXCEPTION_MAXIMUM_PARAMETERS,
                                              0x5a5a5a5a5a5a5a5a);
  exception_writer.SetExceptionInformation(exception_information);

  exception_information.clear();
  exception_information.push_back(kExceptionInformation0);
  exception_information.push_back(kExceptionInformation1);
  exception_information.push_back(kExceptionInformation2);
  exception_writer.SetExceptionInformation(exception_information);

  minidump_file_writer.AddStream(&exception_writer);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&file_writer));

  const MINIDUMP_EXCEPTION_STREAM* observed_exception_stream;
  ASSERT_NO_FATAL_FAILURE(
      GetExceptionStream(file_writer.string(), &observed_exception_stream));

  MINIDUMP_EXCEPTION_STREAM expected_exception_stream = {};
  expected_exception_stream.ThreadId = kThreadID;
  expected_exception_stream.ExceptionRecord.ExceptionCode = kExceptionCode;
  expected_exception_stream.ExceptionRecord.ExceptionFlags = kExceptionFlags;
  expected_exception_stream.ExceptionRecord.ExceptionRecord = kExceptionRecord;
  expected_exception_stream.ExceptionRecord.ExceptionAddress =
      kExceptionAddress;
  expected_exception_stream.ExceptionRecord.NumberParameters =
      exception_information.size();
  for (size_t index = 0; index < exception_information.size(); ++index) {
    expected_exception_stream.ExceptionRecord.ExceptionInformation[index] =
        exception_information[index];
  }
  expected_exception_stream.ThreadContext.DataSize = sizeof(MinidumpContextX86);

  const MinidumpContextX86* observed_context;
  ASSERT_NO_FATAL_FAILURE(ExpectExceptionStream(&expected_exception_stream,
                                                observed_exception_stream,
                                                file_writer.string(),
                                                &observed_context));

  ASSERT_NO_FATAL_FAILURE(ExpectMinidumpContextX86(kSeed, observed_context));
}

TEST(MinidumpExceptionWriterDeathTest, NoContext) {
  MinidumpFileWriter minidump_file_writer;
  MinidumpExceptionWriter exception_writer;

  minidump_file_writer.AddStream(&exception_writer);

  StringFileWriter file_writer;
  ASSERT_DEATH(minidump_file_writer.WriteEverything(&file_writer), "context_");
}

TEST(MinidumpExceptionWriterDeathTest, TooMuchInformation) {
  MinidumpExceptionWriter exception_writer;
  std::vector<uint64_t> exception_information(EXCEPTION_MAXIMUM_PARAMETERS + 1,
                                              0x5a5a5a5a5a5a5a5a);
  ASSERT_DEATH(exception_writer.SetExceptionInformation(exception_information),
               "kMaxParameters");
}

}  // namespace
}  // namespace test
}  // namespace crashpad
