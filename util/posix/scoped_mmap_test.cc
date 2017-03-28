// Copyright 2017 The Crashpad Authors. All rights reserved.
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

#include "util/posix/scoped_mmap.h"

#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "gtest/gtest.h"

namespace crashpad {
namespace test {
namespace {

bool ScopedMmapResetMmap(ScopedMmap* mapping, size_t len) {
  return mapping->ResetMmap(
      nullptr, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
}

// A weird class. This is used to test that memory-mapped regions are freed
// as expected by calling munmap(). This is difficult to test well because once
// a region has been unmapped, the address space it formerly occupied becomes
// eligible for reuse.
//
// The strategy taken here is that a 64-bit cookie value is written into a
// mapped region by SetUp(). While the mapping is active, Check() should
// succeed. After the region is unmapped, calling Check() should fail, either
// because the region has been unmapped and the address not reused, the address
// has been reused but is protected against reading (unlikely), or because the
// address has been reused but the cookie value is no longer present there.
class TestCookie {
 public:
  // A weird constructor for a weird class. The member variable initialization
  // assures that Check() won’t crash if called on an object that hasn’t had
  // SetUp() called on it.
  explicit TestCookie() : address_(&cookie_), cookie_(0) {}

  ~TestCookie() {}

  void SetUp(uint64_t* address) {
    address_ = address, cookie_ = base::RandUint64();
    *address_ = cookie_;
  }

  void Check() {
    if (*address_ != cookie_) {
      __builtin_trap();
    }
  }

 private:
  uint64_t* address_;
  uint64_t cookie_;

  DISALLOW_COPY_AND_ASSIGN(TestCookie);
};

TEST(ScopedMmap, Mmap) {
  TestCookie cookie;

  ScopedMmap mapping;
  EXPECT_FALSE(mapping.is_valid());
  EXPECT_EQ(MAP_FAILED, mapping.addr());
  EXPECT_EQ(0u, mapping.len());

  mapping.Reset();
  EXPECT_FALSE(mapping.is_valid());

  const size_t kPageSize = base::checked_cast<size_t>(getpagesize());
  ASSERT_TRUE(ScopedMmapResetMmap(&mapping, kPageSize));
  EXPECT_TRUE(mapping.is_valid());
  EXPECT_NE(MAP_FAILED, mapping.addr());
  EXPECT_EQ(kPageSize, mapping.len());

  cookie.SetUp(mapping.addr_as<uint64_t*>());
  cookie.Check();

  mapping.Reset();
  EXPECT_FALSE(mapping.is_valid());
}

TEST(ScopedMmapDeathTest, Destructor) {
  TestCookie cookie;
  {
    ScopedMmap mapping;

    const size_t kPageSize = base::checked_cast<size_t>(getpagesize());
    ASSERT_TRUE(ScopedMmapResetMmap(&mapping, kPageSize));
    EXPECT_TRUE(mapping.is_valid());
    EXPECT_NE(MAP_FAILED, mapping.addr());
    EXPECT_EQ(kPageSize, mapping.len());

    cookie.SetUp(mapping.addr_as<uint64_t*>());
  }

  EXPECT_DEATH(cookie.Check(), "");
}

TEST(ScopedMmapDeathTest, Reset) {
  ScopedMmap mapping;

  const size_t kPageSize = base::checked_cast<size_t>(getpagesize());
  ASSERT_TRUE(ScopedMmapResetMmap(&mapping, kPageSize));
  EXPECT_TRUE(mapping.is_valid());
  EXPECT_NE(MAP_FAILED, mapping.addr());
  EXPECT_EQ(kPageSize, mapping.len());

  TestCookie cookie;
  cookie.SetUp(mapping.addr_as<uint64_t*>());

  mapping.Reset();

  EXPECT_DEATH(cookie.Check(), "");
}

TEST(ScopedMmapDeathTest, ResetMmap) {
  ScopedMmap mapping;

  // Calling ScopedMmap::ResetMmap() frees the existing mapping before
  // establishing the new one, so the new one may wind up at the same address as
  // the old. In fact, this is likely. Create a two-page mapping and replace it
  // with a single-page mapping, so that the test can assure that the second
  // page isn’t mapped after establishing the second mapping.
  const size_t kPageSize = base::checked_cast<size_t>(getpagesize());
  ASSERT_TRUE(ScopedMmapResetMmap(&mapping, 2 * kPageSize));
  EXPECT_TRUE(mapping.is_valid());
  EXPECT_NE(MAP_FAILED, mapping.addr());
  EXPECT_EQ(2 * kPageSize, mapping.len());

  TestCookie cookie;
  cookie.SetUp(
      reinterpret_cast<uint64_t*>(mapping.addr_as<char*>() + kPageSize));

  ASSERT_TRUE(ScopedMmapResetMmap(&mapping, kPageSize));
  EXPECT_TRUE(mapping.is_valid());
  EXPECT_NE(MAP_FAILED, mapping.addr());
  EXPECT_EQ(kPageSize, mapping.len());

  EXPECT_DEATH(cookie.Check(), "");
}

TEST(ScopedMmapDeathTest, Mprotect) {
  ScopedMmap mapping;

  const size_t kPageSize = base::checked_cast<size_t>(getpagesize());
  ASSERT_TRUE(ScopedMmapResetMmap(&mapping, kPageSize));
  EXPECT_TRUE(mapping.is_valid());
  EXPECT_NE(MAP_FAILED, mapping.addr());
  EXPECT_EQ(kPageSize, mapping.len());

  char* addr = mapping.addr_as<char*>();
  *addr = 0;

  ASSERT_TRUE(mapping.Mprotect(PROT_READ));

  EXPECT_DEATH(*addr = 0, "");
}

}  // namespace
}  // namespace test
}  // namespace crashpad
