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

#include "minidump/minidump_simple_string_dictionary_writer.h"

#include <string>

#include "gtest/gtest.h"
#include "minidump/minidump_extensions.h"
#include "util/file/string_file_writer.h"

namespace crashpad {
namespace test {
namespace {

const MinidumpSimpleStringDictionary* MinidumpSimpleStringDictionaryCast(
    const StringFileWriter& file_writer) {
  return reinterpret_cast<const MinidumpSimpleStringDictionary*>(
      &file_writer.string()[0]);
}

TEST(MinidumpSimpleStringDictionaryWriter, EmptySimpleStringDictionary) {
  StringFileWriter file_writer;

  MinidumpSimpleStringDictionaryWriter dictionary_writer;

  EXPECT_TRUE(dictionary_writer.WriteEverything(&file_writer));
  ASSERT_EQ(sizeof(MinidumpSimpleStringDictionary),
            file_writer.string().size());

  const MinidumpSimpleStringDictionary* dictionary =
      MinidumpSimpleStringDictionaryCast(file_writer);
  EXPECT_EQ(0u, dictionary->count);
}

std::string MinidumpUTF8StringAtRVA(const StringFileWriter& file_writer,
                                    RVA rva) {
  const std::string& contents = file_writer.string();
  if (rva == 0) {
    return std::string();
  }

  if (rva + sizeof(MinidumpUTF8String) > contents.size()) {
    ADD_FAILURE()
        << "rva " << rva << " too large for contents " << contents.size();
    return std::string();
  }

  const MinidumpUTF8String* minidump_string =
      reinterpret_cast<const MinidumpUTF8String*>(&contents[rva]);

  // Verify that the file has enough data for the string’s stated length plus
  // its required NUL terminator.
  if (rva + sizeof(MinidumpUTF8String) + minidump_string->Length + 1 >
          contents.size()) {
    ADD_FAILURE()
        << "rva " << rva << ", length " << minidump_string->Length
        << " too large for contents " << contents.size();
    return std::string();
  }

  std::string minidump_string_data(
      reinterpret_cast<const char*>(&minidump_string->Buffer[0]),
      minidump_string->Length);
  return minidump_string_data;
}

TEST(MinidumpSimpleStringDictionaryWriter, EmptyKeyValue) {
  StringFileWriter file_writer;

  MinidumpSimpleStringDictionaryWriter dictionary_writer;
  MinidumpSimpleStringDictionaryEntryWriter entry_writer;
  dictionary_writer.AddEntry(&entry_writer);

  EXPECT_TRUE(dictionary_writer.WriteEverything(&file_writer));
  ASSERT_EQ(sizeof(MinidumpSimpleStringDictionary) +
                sizeof(MinidumpSimpleStringDictionaryEntry) +
                2 * sizeof(MinidumpUTF8String) + 1 + 3 + 1,  // 3 for padding
            file_writer.string().size());

  const MinidumpSimpleStringDictionary* dictionary =
      MinidumpSimpleStringDictionaryCast(file_writer);
  EXPECT_EQ(1u, dictionary->count);
  EXPECT_EQ(12u, dictionary->entries[0].key);
  EXPECT_EQ(20u, dictionary->entries[0].value);
  EXPECT_EQ("",
            MinidumpUTF8StringAtRVA(file_writer, dictionary->entries[0].key));
  EXPECT_EQ("",
            MinidumpUTF8StringAtRVA(file_writer, dictionary->entries[0].value));
}

TEST(MinidumpSimpleStringDictionaryWriter, OneKeyValue) {
  StringFileWriter file_writer;

  char kKey[] = "key";
  char kValue[] = "value";

  MinidumpSimpleStringDictionaryWriter dictionary_writer;
  MinidumpSimpleStringDictionaryEntryWriter entry_writer;
  entry_writer.SetKeyValue(kKey, kValue);
  dictionary_writer.AddEntry(&entry_writer);

  EXPECT_TRUE(dictionary_writer.WriteEverything(&file_writer));
  ASSERT_EQ(sizeof(MinidumpSimpleStringDictionary) +
                sizeof(MinidumpSimpleStringDictionaryEntry) +
                2 * sizeof(MinidumpUTF8String) + sizeof(kKey) + sizeof(kValue),
            file_writer.string().size());

  const MinidumpSimpleStringDictionary* dictionary =
      MinidumpSimpleStringDictionaryCast(file_writer);
  EXPECT_EQ(1u, dictionary->count);
  EXPECT_EQ(12u, dictionary->entries[0].key);
  EXPECT_EQ(20u, dictionary->entries[0].value);
  EXPECT_EQ(kKey,
            MinidumpUTF8StringAtRVA(file_writer, dictionary->entries[0].key));
  EXPECT_EQ(kValue,
            MinidumpUTF8StringAtRVA(file_writer, dictionary->entries[0].value));
}

TEST(MinidumpSimpleStringDictionaryWriter, ThreeKeysValues) {
  StringFileWriter file_writer;

  char kKey0[] = "m0";
  char kValue0[] = "value0";
  char kKey1[] = "zzz1";
  char kValue1[] = "v1";
  char kKey2[] = "aa2";
  char kValue2[] = "val2";

  MinidumpSimpleStringDictionaryWriter dictionary_writer;
  MinidumpSimpleStringDictionaryEntryWriter entry_writer_0;
  entry_writer_0.SetKeyValue(kKey0, kValue0);
  dictionary_writer.AddEntry(&entry_writer_0);
  MinidumpSimpleStringDictionaryEntryWriter entry_writer_1;
  entry_writer_1.SetKeyValue(kKey1, kValue1);
  dictionary_writer.AddEntry(&entry_writer_1);
  MinidumpSimpleStringDictionaryEntryWriter entry_writer_2;
  entry_writer_2.SetKeyValue(kKey2, kValue2);
  dictionary_writer.AddEntry(&entry_writer_2);

  EXPECT_TRUE(dictionary_writer.WriteEverything(&file_writer));
  ASSERT_EQ(sizeof(MinidumpSimpleStringDictionary) +
                3 * sizeof(MinidumpSimpleStringDictionaryEntry) +
                6 * sizeof(MinidumpUTF8String) + sizeof(kKey2) +
                sizeof(kValue2) + 3 + sizeof(kKey0) + 1 + sizeof(kValue0) + 1 +
                sizeof(kKey1) + 3 + sizeof(kValue1),
            file_writer.string().size());

  const MinidumpSimpleStringDictionary* dictionary =
      MinidumpSimpleStringDictionaryCast(file_writer);
  EXPECT_EQ(3u, dictionary->count);
  EXPECT_EQ(28u, dictionary->entries[0].key);
  EXPECT_EQ(36u, dictionary->entries[0].value);
  EXPECT_EQ(48u, dictionary->entries[1].key);
  EXPECT_EQ(56u, dictionary->entries[1].value);
  EXPECT_EQ(68u, dictionary->entries[2].key);
  EXPECT_EQ(80u, dictionary->entries[2].value);

  // The entries don’t appear in the order they were added. The current
  // implementation uses a std::map and sorts keys, so the entires appear in
  // alphabetical order. However, this is an implementation detail, and it’s OK
  // if the writer stops sorting in this order. Testing for a specific order is
  // just the easiest way to write this test while the writer will output things
  // in a known order.
  EXPECT_EQ(kKey2,
            MinidumpUTF8StringAtRVA(file_writer, dictionary->entries[0].key));
  EXPECT_EQ(kValue2,
            MinidumpUTF8StringAtRVA(file_writer, dictionary->entries[0].value));
  EXPECT_EQ(kKey0,
            MinidumpUTF8StringAtRVA(file_writer, dictionary->entries[1].key));
  EXPECT_EQ(kValue0,
            MinidumpUTF8StringAtRVA(file_writer, dictionary->entries[1].value));
  EXPECT_EQ(kKey1,
            MinidumpUTF8StringAtRVA(file_writer, dictionary->entries[2].key));
  EXPECT_EQ(kValue1,
            MinidumpUTF8StringAtRVA(file_writer, dictionary->entries[2].value));
}

TEST(MinidumpSimpleStringDictionaryWriter, DuplicateKeyValue) {
  StringFileWriter file_writer;

  char kKey[] = "key";
  char kValue0[] = "fake_value";
  char kValue1[] = "value";

  MinidumpSimpleStringDictionaryWriter dictionary_writer;
  MinidumpSimpleStringDictionaryEntryWriter entry_writer_0;
  entry_writer_0.SetKeyValue(kKey, kValue0);
  dictionary_writer.AddEntry(&entry_writer_0);
  MinidumpSimpleStringDictionaryEntryWriter entry_writer_1;
  entry_writer_1.SetKeyValue(kKey, kValue1);
  dictionary_writer.AddEntry(&entry_writer_1);

  EXPECT_TRUE(dictionary_writer.WriteEverything(&file_writer));
  ASSERT_EQ(sizeof(MinidumpSimpleStringDictionary) +
                sizeof(MinidumpSimpleStringDictionaryEntry) +
                2 * sizeof(MinidumpUTF8String) + sizeof(kKey) + sizeof(kValue1),
            file_writer.string().size());

  const MinidumpSimpleStringDictionary* dictionary =
      MinidumpSimpleStringDictionaryCast(file_writer);
  EXPECT_EQ(1u, dictionary->count);
  EXPECT_EQ(12u, dictionary->entries[0].key);
  EXPECT_EQ(20u, dictionary->entries[0].value);
  EXPECT_EQ(kKey,
            MinidumpUTF8StringAtRVA(file_writer, dictionary->entries[0].key));
  EXPECT_EQ(kValue1,
            MinidumpUTF8StringAtRVA(file_writer, dictionary->entries[0].value));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
