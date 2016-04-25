// Copyright 2015 The Crashpad Authors. All rights reserved.
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

#include "minidump/minidump_handle_writer.h"

#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "gtest/gtest.h"
#include "minidump/minidump_file_writer.h"
#include "minidump/test/minidump_file_writer_test_util.h"
#include "minidump/test/minidump_string_writer_test_util.h"
#include "minidump/test/minidump_writable_test_util.h"
#include "util/file/string_file.h"

namespace crashpad {
namespace test {
namespace {

// The handle data stream is expected to be the only stream.
void GetHandleDataStream(
    const std::string& file_contents,
    const MINIDUMP_HANDLE_DATA_STREAM** handle_data_stream) {
  const size_t kDirectoryOffset = sizeof(MINIDUMP_HEADER);
  const size_t kHandleDataStreamOffset =
      kDirectoryOffset + sizeof(MINIDUMP_DIRECTORY);

  const MINIDUMP_DIRECTORY* directory;
  const MINIDUMP_HEADER* header =
      MinidumpHeaderAtStart(file_contents, &directory);
  ASSERT_NO_FATAL_FAILURE(VerifyMinidumpHeader(header, 1, 0));
  ASSERT_TRUE(directory);

  const size_t kDirectoryIndex = 0;

  ASSERT_EQ(kMinidumpStreamTypeHandleData,
            directory[kDirectoryIndex].StreamType);
  EXPECT_EQ(kHandleDataStreamOffset, directory[kDirectoryIndex].Location.Rva);

  *handle_data_stream =
      MinidumpWritableAtLocationDescriptor<MINIDUMP_HANDLE_DATA_STREAM>(
          file_contents, directory[kDirectoryIndex].Location);
  ASSERT_TRUE(*handle_data_stream);
}

TEST(MinidumpHandleDataWriter, Empty) {
  MinidumpFileWriter minidump_file_writer;
  auto handle_data_writer = base::WrapUnique(new MinidumpHandleDataWriter());
  minidump_file_writer.AddStream(std::move(handle_data_writer));

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  ASSERT_EQ(sizeof(MINIDUMP_HEADER) + sizeof(MINIDUMP_DIRECTORY) +
                sizeof(MINIDUMP_HANDLE_DATA_STREAM),
            string_file.string().size());

  const MINIDUMP_HANDLE_DATA_STREAM* handle_data_stream = nullptr;
  ASSERT_NO_FATAL_FAILURE(
      GetHandleDataStream(string_file.string(), &handle_data_stream));

  EXPECT_EQ(0u, handle_data_stream->NumberOfDescriptors);
}

TEST(MinidumpHandleDataWriter, OneHandle) {
  MinidumpFileWriter minidump_file_writer;
  auto handle_data_writer = base::WrapUnique(new MinidumpHandleDataWriter());

  HandleSnapshot handle_snapshot;
  handle_snapshot.handle = 0x1234;
  handle_snapshot.type_name = "Something";
  handle_snapshot.attributes = 0x12345678;
  handle_snapshot.granted_access = 0x9abcdef0;
  handle_snapshot.pointer_count = 4567;
  handle_snapshot.handle_count = 9876;

  std::vector<HandleSnapshot> snapshot;
  snapshot.push_back(handle_snapshot);

  handle_data_writer->InitializeFromSnapshot(snapshot);

  minidump_file_writer.AddStream(std::move(handle_data_writer));

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  const size_t kTypeNameStringDataLength =
      (handle_snapshot.type_name.size() + 1) * sizeof(base::char16);
  ASSERT_EQ(sizeof(MINIDUMP_HEADER) + sizeof(MINIDUMP_DIRECTORY) +
                sizeof(MINIDUMP_HANDLE_DATA_STREAM) +
                sizeof(MINIDUMP_HANDLE_DESCRIPTOR) + sizeof(MINIDUMP_STRING) +
                kTypeNameStringDataLength,
            string_file.string().size());

  const MINIDUMP_HANDLE_DATA_STREAM* handle_data_stream = nullptr;
  ASSERT_NO_FATAL_FAILURE(
      GetHandleDataStream(string_file.string(), &handle_data_stream));

  EXPECT_EQ(1u, handle_data_stream->NumberOfDescriptors);
  const MINIDUMP_HANDLE_DESCRIPTOR* handle_descriptor =
      reinterpret_cast<const MINIDUMP_HANDLE_DESCRIPTOR*>(
          &handle_data_stream[1]);
  EXPECT_EQ(handle_snapshot.handle, handle_descriptor->Handle);
  EXPECT_EQ(handle_snapshot.type_name,
            base::UTF16ToUTF8(MinidumpStringAtRVAAsString(
                string_file.string(), handle_descriptor->TypeNameRva)));
  EXPECT_EQ(0u, handle_descriptor->ObjectNameRva);
  EXPECT_EQ(handle_snapshot.attributes, handle_descriptor->Attributes);
  EXPECT_EQ(handle_snapshot.granted_access, handle_descriptor->GrantedAccess);
  EXPECT_EQ(handle_snapshot.handle_count, handle_descriptor->HandleCount);
  EXPECT_EQ(handle_snapshot.pointer_count, handle_descriptor->PointerCount);
}

TEST(MinidumpHandleDataWriter, RepeatedTypeName) {
  MinidumpFileWriter minidump_file_writer;
  auto handle_data_writer = base::WrapUnique(new MinidumpHandleDataWriter());

  HandleSnapshot handle_snapshot;
  handle_snapshot.handle = 0x1234;
  handle_snapshot.type_name = "Something";
  handle_snapshot.attributes = 0x12345678;
  handle_snapshot.granted_access = 0x9abcdef0;
  handle_snapshot.pointer_count = 4567;
  handle_snapshot.handle_count = 9876;

  HandleSnapshot handle_snapshot2;
  handle_snapshot2.handle = 0x4321;
  handle_snapshot2.type_name = "Something";  // Note: same as above.
  handle_snapshot2.attributes = 0x87654321;
  handle_snapshot2.granted_access = 0x0fedcba9;
  handle_snapshot2.pointer_count = 7654;
  handle_snapshot2.handle_count = 6789;

  std::vector<HandleSnapshot> snapshot;
  snapshot.push_back(handle_snapshot);
  snapshot.push_back(handle_snapshot2);

  handle_data_writer->InitializeFromSnapshot(snapshot);

  minidump_file_writer.AddStream(std::move(handle_data_writer));

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  const size_t kTypeNameStringDataLength =
      (handle_snapshot.type_name.size() + 1) * sizeof(base::char16);
  ASSERT_EQ(sizeof(MINIDUMP_HEADER) + sizeof(MINIDUMP_DIRECTORY) +
                sizeof(MINIDUMP_HANDLE_DATA_STREAM) +
                (sizeof(MINIDUMP_HANDLE_DESCRIPTOR) * 2) +
                sizeof(MINIDUMP_STRING) + kTypeNameStringDataLength,
            string_file.string().size());

  const MINIDUMP_HANDLE_DATA_STREAM* handle_data_stream = nullptr;
  ASSERT_NO_FATAL_FAILURE(
      GetHandleDataStream(string_file.string(), &handle_data_stream));

  EXPECT_EQ(2u, handle_data_stream->NumberOfDescriptors);
  const MINIDUMP_HANDLE_DESCRIPTOR* handle_descriptor =
      reinterpret_cast<const MINIDUMP_HANDLE_DESCRIPTOR*>(
          &handle_data_stream[1]);
  EXPECT_EQ(handle_snapshot.handle, handle_descriptor->Handle);
  EXPECT_EQ(handle_snapshot.type_name,
            base::UTF16ToUTF8(MinidumpStringAtRVAAsString(
                string_file.string(), handle_descriptor->TypeNameRva)));
  EXPECT_EQ(0u, handle_descriptor->ObjectNameRva);
  EXPECT_EQ(handle_snapshot.attributes, handle_descriptor->Attributes);
  EXPECT_EQ(handle_snapshot.granted_access, handle_descriptor->GrantedAccess);
  EXPECT_EQ(handle_snapshot.handle_count, handle_descriptor->HandleCount);
  EXPECT_EQ(handle_snapshot.pointer_count, handle_descriptor->PointerCount);

  const MINIDUMP_HANDLE_DESCRIPTOR* handle_descriptor2 =
      reinterpret_cast<const MINIDUMP_HANDLE_DESCRIPTOR*>(
          reinterpret_cast<const unsigned char*>(&handle_data_stream[1]) +
          sizeof(MINIDUMP_HANDLE_DESCRIPTOR));
  EXPECT_EQ(handle_snapshot2.handle, handle_descriptor2->Handle);
  EXPECT_EQ(handle_snapshot2.type_name,
            base::UTF16ToUTF8(MinidumpStringAtRVAAsString(
                string_file.string(), handle_descriptor2->TypeNameRva)));
  EXPECT_EQ(0u, handle_descriptor2->ObjectNameRva);
  EXPECT_EQ(handle_snapshot2.attributes, handle_descriptor2->Attributes);
  EXPECT_EQ(handle_snapshot2.granted_access, handle_descriptor2->GrantedAccess);
  EXPECT_EQ(handle_snapshot2.handle_count, handle_descriptor2->HandleCount);
  EXPECT_EQ(handle_snapshot2.pointer_count, handle_descriptor2->PointerCount);

  EXPECT_EQ(handle_descriptor->TypeNameRva, handle_descriptor2->TypeNameRva);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
