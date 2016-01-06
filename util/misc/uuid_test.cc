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

#include "util/misc/uuid.h"

#include <string.h>
#include <sys/types.h>

#include <string>

#include "base/format_macros.h"
#include "base/macros.h"
#include "base/scoped_generic.h"
#include "base/strings/stringprintf.h"
#include "gtest/gtest.h"

namespace crashpad {
namespace test {
namespace {

TEST(UUID, UUID) {
  UUID uuid_zero;
  EXPECT_EQ(0u, uuid_zero.data_1);
  EXPECT_EQ(0u, uuid_zero.data_2);
  EXPECT_EQ(0u, uuid_zero.data_3);
  EXPECT_EQ(0u, uuid_zero.data_4[0]);
  EXPECT_EQ(0u, uuid_zero.data_4[1]);
  EXPECT_EQ(0u, uuid_zero.data_5[0]);
  EXPECT_EQ(0u, uuid_zero.data_5[1]);
  EXPECT_EQ(0u, uuid_zero.data_5[2]);
  EXPECT_EQ(0u, uuid_zero.data_5[3]);
  EXPECT_EQ(0u, uuid_zero.data_5[4]);
  EXPECT_EQ(0u, uuid_zero.data_5[5]);
  EXPECT_EQ("00000000-0000-0000-0000-000000000000", uuid_zero.ToString());

  const uint8_t kBytes[16] = {0x00,
                              0x01,
                              0x02,
                              0x03,
                              0x04,
                              0x05,
                              0x06,
                              0x07,
                              0x08,
                              0x09,
                              0x0a,
                              0x0b,
                              0x0c,
                              0x0d,
                              0x0e,
                              0x0f};
  UUID uuid(kBytes);
  EXPECT_EQ(0x00010203u, uuid.data_1);
  EXPECT_EQ(0x0405u, uuid.data_2);
  EXPECT_EQ(0x0607u, uuid.data_3);
  EXPECT_EQ(0x08u, uuid.data_4[0]);
  EXPECT_EQ(0x09u, uuid.data_4[1]);
  EXPECT_EQ(0x0au, uuid.data_5[0]);
  EXPECT_EQ(0x0bu, uuid.data_5[1]);
  EXPECT_EQ(0x0cu, uuid.data_5[2]);
  EXPECT_EQ(0x0du, uuid.data_5[3]);
  EXPECT_EQ(0x0eu, uuid.data_5[4]);
  EXPECT_EQ(0x0fu, uuid.data_5[5]);
  EXPECT_EQ("00010203-0405-0607-0809-0a0b0c0d0e0f", uuid.ToString());

  // Test both operator== and operator!=.
  EXPECT_FALSE(uuid == uuid_zero);
  EXPECT_NE(uuid, uuid_zero);

  UUID uuid_2(kBytes);
  EXPECT_EQ(uuid, uuid_2);
  EXPECT_FALSE(uuid != uuid_2);

  // Make sure that operator== and operator!= check the entire UUID.
  ++uuid.data_1;
  EXPECT_NE(uuid, uuid_2);
  --uuid.data_1;
  ++uuid.data_2;
  EXPECT_NE(uuid, uuid_2);
  --uuid.data_2;
  ++uuid.data_3;
  EXPECT_NE(uuid, uuid_2);
  --uuid.data_3;
  for (size_t index = 0; index < arraysize(uuid.data_4); ++index) {
    ++uuid.data_4[index];
    EXPECT_NE(uuid, uuid_2);
    --uuid.data_4[index];
  }
  for (size_t index = 0; index < arraysize(uuid.data_5); ++index) {
    ++uuid.data_5[index];
    EXPECT_NE(uuid, uuid_2);
    --uuid.data_5[index];
  }

  // Make sure that the UUIDs are equal again, otherwise the test above may not
  // have been valid.
  EXPECT_EQ(uuid, uuid_2);

  const uint8_t kMoreBytes[16] = {0xff,
                                  0xee,
                                  0xdd,
                                  0xcc,
                                  0xbb,
                                  0xaa,
                                  0x99,
                                  0x88,
                                  0x77,
                                  0x66,
                                  0x55,
                                  0x44,
                                  0x33,
                                  0x22,
                                  0x11,
                                  0x00};
  uuid.InitializeFromBytes(kMoreBytes);
  EXPECT_EQ(0xffeeddccu, uuid.data_1);
  EXPECT_EQ(0xbbaau, uuid.data_2);
  EXPECT_EQ(0x9988u, uuid.data_3);
  EXPECT_EQ(0x77u, uuid.data_4[0]);
  EXPECT_EQ(0x66u, uuid.data_4[1]);
  EXPECT_EQ(0x55u, uuid.data_5[0]);
  EXPECT_EQ(0x44u, uuid.data_5[1]);
  EXPECT_EQ(0x33u, uuid.data_5[2]);
  EXPECT_EQ(0x22u, uuid.data_5[3]);
  EXPECT_EQ(0x11u, uuid.data_5[4]);
  EXPECT_EQ(0x00u, uuid.data_5[5]);
  EXPECT_EQ("ffeeddcc-bbaa-9988-7766-554433221100", uuid.ToString());

  EXPECT_NE(uuid, uuid_2);
  EXPECT_NE(uuid, uuid_zero);

  // Test that UUID is standard layout.
  memset(&uuid, 0x45, 16);
  EXPECT_EQ(0x45454545u, uuid.data_1);
  EXPECT_EQ(0x4545u, uuid.data_2);
  EXPECT_EQ(0x4545u, uuid.data_3);
  EXPECT_EQ(0x45u, uuid.data_4[0]);
  EXPECT_EQ(0x45u, uuid.data_4[1]);
  EXPECT_EQ(0x45u, uuid.data_5[0]);
  EXPECT_EQ(0x45u, uuid.data_5[1]);
  EXPECT_EQ(0x45u, uuid.data_5[2]);
  EXPECT_EQ(0x45u, uuid.data_5[3]);
  EXPECT_EQ(0x45u, uuid.data_5[4]);
  EXPECT_EQ(0x45u, uuid.data_5[5]);
  EXPECT_EQ("45454545-4545-4545-4545-454545454545", uuid.ToString());

  UUID initialized_generated(UUID::InitializeWithNewTag{});
  EXPECT_NE(initialized_generated, uuid_zero);
}

TEST(UUID, FromString) {
  const struct TestCase {
    const char* uuid_string;
    bool success;
  } kCases[] = {
    // Valid:
    {"c6849cb5-fe14-4a79-8978-9ae6034c521d", true},
    {"00000000-0000-0000-0000-000000000000", true},
    {"ffffffff-ffff-ffff-ffff-ffffffffffff", true},
    // Outside HEX range:
    {"7318z10b-c453-4cef-9dc8-015655cb4bbc", false},
    {"7318a10b-c453-4cef-9dz8-015655cb4bbc", false},
    // Incomplete:
    {"15655cb4-", false},
    {"7318f10b-c453-4cef-9dc8-015655cb4bb", false},
    {"318f10b-c453-4cef-9dc8-015655cb4bb2", false},
    {"7318f10b-c453-4ef-9dc8-015655cb4bb2", false},
    {"", false},
    {"abcd", false},
    // Trailing data:
    {"6d247a34-53d5-40ec-a90d-d8dea9e94cc01", false}
  };

  const std::string empty_uuid = UUID().ToString();

  for (size_t index = 0; index < arraysize(kCases); ++index) {
    const TestCase& test_case = kCases[index];
    SCOPED_TRACE(base::StringPrintf(
        "index %" PRIuS ": %s", index, test_case.uuid_string));

    UUID uuid;
    EXPECT_EQ(test_case.success,
              uuid.InitializeFromString(test_case.uuid_string));
    if (test_case.success) {
      EXPECT_EQ(test_case.uuid_string, uuid.ToString());
    } else {
      EXPECT_EQ(empty_uuid, uuid.ToString());
    }
  }

  // Test for case insensitivty.
  UUID uuid;
  uuid.InitializeFromString("F32E5BDC-2681-4C73-A4E6-911FFD89B846");
  EXPECT_EQ("f32e5bdc-2681-4c73-a4e6-911ffd89b846", uuid.ToString());

  // Mixed case.
  uuid.InitializeFromString("5762C15D-50b5-4171-a2e9-7429C9EC6CAB");
  EXPECT_EQ("5762c15d-50b5-4171-a2e9-7429c9ec6cab", uuid.ToString());
}

#if defined(OS_WIN)

TEST(UUID, FromSystem) {
  ::GUID system_uuid;
  ASSERT_EQ(RPC_S_OK, UuidCreate(&system_uuid));

  UUID uuid;
  uuid.InitializeFromSystemUUID(&system_uuid);

  RPC_WSTR system_string;
  ASSERT_EQ(RPC_S_OK, UuidToString(&system_uuid, &system_string));

  struct ScopedRpcStringFreeTraits {
    static RPC_WSTR* InvalidValue() { return nullptr; }
    static void Free(RPC_WSTR* rpc_string) {
      EXPECT_EQ(RPC_S_OK, RpcStringFree(rpc_string));
    }
  };
  using ScopedRpcString =
      base::ScopedGeneric<RPC_WSTR*, ScopedRpcStringFreeTraits>;
  ScopedRpcString scoped_system_string(&system_string);

  EXPECT_EQ(reinterpret_cast<wchar_t*>(system_string), uuid.ToString16());
}

#endif  // OS_WIN

}  // namespace
}  // namespace test
}  // namespace crashpad
