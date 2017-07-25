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

#include "snapshot/mac/system_snapshot_mac.h"

#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include <string>

#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "gtest/gtest.h"
#include "snapshot/mac/process_reader.h"
#include "test/errors.h"
#include "util/mac/mac_util.h"

namespace crashpad {
namespace test {
namespace {

// SystemSnapshotMac objects would be cumbersome to construct in each test that
// requires one, because of the repetitive and mechanical work necessary to set
// up a ProcessReader and timeval, along with the checks to verify that these
// operations succeed. This test fixture class handles the initialization work
// so that individual tests don’t have to.
class SystemSnapshotMacTest : public testing::Test {
 public:
  SystemSnapshotMacTest()
      : Test(),
        process_reader_(),
        snapshot_time_(),
        system_snapshot_() {
  }

  const internal::SystemSnapshotMac& system_snapshot() const {
    return system_snapshot_;
  }

  // testing::Test:
  void SetUp() override {
    ASSERT_TRUE(process_reader_.Initialize(mach_task_self()));
    ASSERT_EQ(gettimeofday(&snapshot_time_, nullptr), 0)
        << ErrnoMessage("gettimeofday");
    system_snapshot_.Initialize(&process_reader_, &snapshot_time_);
  }

 private:
  ProcessReader process_reader_;
  timeval snapshot_time_;
  internal::SystemSnapshotMac system_snapshot_;

  DISALLOW_COPY_AND_ASSIGN(SystemSnapshotMacTest);
};

TEST_F(SystemSnapshotMacTest, GetCPUArchitecture) {
  CPUArchitecture cpu_architecture = system_snapshot().GetCPUArchitecture();

#if defined(ARCH_CPU_X86)
  EXPECT_EQ(cpu_architecture, kCPUArchitectureX86);
#elif defined(ARCH_CPU_X86_64)
  EXPECT_EQ(cpu_architecture, kCPUArchitectureX86_64);
#else
#error port to your architecture
#endif
}

TEST_F(SystemSnapshotMacTest, CPUCount) {
  EXPECT_GE(system_snapshot().CPUCount(), 1);
}

TEST_F(SystemSnapshotMacTest, CPUVendor) {
  std::string cpu_vendor = system_snapshot().CPUVendor();

#if defined(ARCH_CPU_X86_FAMILY)
  // Apple has only shipped Intel x86-family CPUs, but here’s a small nod to the
  // “Hackintosh” crowd.
  if (cpu_vendor != "GenuineIntel" && cpu_vendor != "AuthenticAMD") {
    FAIL() << "cpu_vendor " << cpu_vendor;
  }
#else
#error port to your architecture
#endif
}

#if defined(ARCH_CPU_X86_FAMILY)

TEST_F(SystemSnapshotMacTest, CPUX86SupportsDAZ) {
  // All x86-family CPUs that Apple is known to have shipped should support DAZ.
  EXPECT_TRUE(system_snapshot().CPUX86SupportsDAZ());
}

#endif

TEST_F(SystemSnapshotMacTest, GetOperatingSystem) {
  EXPECT_EQ(system_snapshot().GetOperatingSystem(),
            SystemSnapshot::kOperatingSystemMacOSX);
}

TEST_F(SystemSnapshotMacTest, OSVersion) {
  int major;
  int minor;
  int bugfix;
  std::string build;
  system_snapshot().OSVersion(&major, &minor, &bugfix, &build);

  EXPECT_EQ(major, 10);
  EXPECT_EQ(minor, MacOSXMinorVersion());
  EXPECT_FALSE(build.empty());
}

TEST_F(SystemSnapshotMacTest, OSVersionFull) {
  EXPECT_FALSE(system_snapshot().OSVersionFull().empty());
}

TEST_F(SystemSnapshotMacTest, MachineDescription) {
  EXPECT_FALSE(system_snapshot().MachineDescription().empty());
}

class ScopedSetTZ {
 public:
  ScopedSetTZ(const std::string& tz) {
    const char* old_tz = getenv(kTZ);
    old_tz_set_ = old_tz;
    if (old_tz_set_) {
      old_tz_.assign(old_tz);
    }

    EXPECT_EQ(setenv(kTZ, tz.c_str(), 1), 0) << ErrnoMessage("setenv");
    tzset();
  }

  ~ScopedSetTZ() {
    if (old_tz_set_) {
      EXPECT_EQ(setenv(kTZ, old_tz_.c_str(), 1), 0) << ErrnoMessage("setenv");
    } else {
      EXPECT_EQ(unsetenv(kTZ), 0) << ErrnoMessage("unsetenv");
    }
    tzset();
  }

 private:
  std::string old_tz_;
  bool old_tz_set_;

  static constexpr char kTZ[] = "TZ";

  DISALLOW_COPY_AND_ASSIGN(ScopedSetTZ);
};

constexpr char ScopedSetTZ::kTZ[];

TEST_F(SystemSnapshotMacTest, TimeZone) {
  SystemSnapshot::DaylightSavingTimeStatus dst_status;
  int standard_offset_seconds;
  int daylight_offset_seconds;
  std::string standard_name;
  std::string daylight_name;

  system_snapshot().TimeZone(&dst_status,
                             &standard_offset_seconds,
                             &daylight_offset_seconds,
                             &standard_name,
                             &daylight_name);

  // |standard_offset_seconds| gives seconds east of UTC, and |timezone| gives
  // seconds west of UTC.
  EXPECT_EQ(standard_offset_seconds, -timezone);

  // In contemporary usage, most time zones have an integer hour offset from
  // UTC, although several are at a half-hour offset, and two are at 15-minute
  // offsets. Throughout history, other variations existed. See
  // http://www.timeanddate.com/time/time-zones-interesting.html.
  EXPECT_EQ(standard_offset_seconds % (15 * 60), 0)
      << "standard_offset_seconds " << standard_offset_seconds;

  if (dst_status == SystemSnapshot::kDoesNotObserveDaylightSavingTime) {
    EXPECT_EQ(daylight_offset_seconds, standard_offset_seconds);
    EXPECT_EQ(daylight_name, standard_name);
  } else {
    EXPECT_EQ(daylight_offset_seconds % (15 * 60), 0)
        << "daylight_offset_seconds " << daylight_offset_seconds;

    // In contemporary usage, dst_delta_seconds will almost always be one hour,
    // except for Lord Howe Island, Australia, which uses a 30-minute
    // delta. Throughout history, other variations existed. See
    // http://www.timeanddate.com/time/dst/#brief.
    int dst_delta_seconds = daylight_offset_seconds - standard_offset_seconds;
    if (dst_delta_seconds != 60 * 60 && dst_delta_seconds != 30 * 60) {
      FAIL() << "dst_delta_seconds " << dst_delta_seconds;
    }

    EXPECT_NE(standard_name, daylight_name);
  }

  // Test a variety of time zones. Some of these observe daylight saving time,
  // some don’t. Some used to but no longer do. Some have uncommon UTC offsets.
  // standard_name and daylight_name can be nullptr where no name exists to
  // verify, as may happen when some versions of the timezone database carry
  // invented names and others do not.
  static constexpr struct {
    const char* tz;
    bool observes_dst;
    float standard_offset_hours;
    float daylight_offset_hours;
    const char* standard_name;
    const char* daylight_name;
  } kTestTimeZones[] = {
      {"America/Anchorage", true, -9, -8, "AKST", "AKDT"},
      {"America/Chicago", true, -6, -5, "CST", "CDT"},
      {"America/Denver", true, -7, -6, "MST", "MDT"},
      {"America/Halifax", true, -4, -3, "AST", "ADT"},
      {"America/Los_Angeles", true, -8, -7, "PST", "PDT"},
      {"America/New_York", true, -5, -4, "EST", "EDT"},
      {"America/Phoenix", false, -7, -7, "MST", "MST"},
      {"Asia/Karachi", false, 5, 5, "PKT", "PKT"},
      {"Asia/Kolkata", false, 5.5, 5.5, "IST", "IST"},
      {"Asia/Shanghai", false, 8, 8, "CST", "CST"},
      {"Asia/Tokyo", false, 9, 9, "JST", "JST"},
      {"Australia/Adelaide", true, 9.5, 10.5, "ACST", "ACDT"},
      {"Australia/Brisbane", false, 10, 10, "AEST", "AEST"},
      {"Australia/Darwin", false, 9.5, 9.5, "ACST", "ACST"},
      {"Australia/Eucla", false, 8.75, 8.75, nullptr, nullptr},
      {"Australia/Lord_Howe", true, 10.5, 11, nullptr, nullptr},
      {"Australia/Perth", false, 8, 8, "AWST", "AWST"},
      {"Australia/Sydney", true, 10, 11, "AEST", "AEDT"},
      {"Europe/Bucharest", true, 2, 3, "EET", "EEST"},
      {"Europe/London", true, 0, 1, "GMT", "BST"},
      {"Europe/Moscow", false, 3, 3, "MSK", "MSK"},
      {"Europe/Paris", true, 1, 2, "CET", "CEST"},
      {"Europe/Reykjavik", false, 0, 0, "UTC", "UTC"},
      {"Pacific/Auckland", true, 12, 13, "NZST", "NZDT"},
      {"Pacific/Honolulu", false, -10, -10, "HST", "HST"},
      {"UTC", false, 0, 0, "UTC", "UTC"},
  };

  for (size_t index = 0; index < arraysize(kTestTimeZones); ++index) {
    const auto& test_time_zone = kTestTimeZones[index];
    const char* tz = test_time_zone.tz;
    SCOPED_TRACE(base::StringPrintf("index %zu, tz %s", index, tz));

    {
      ScopedSetTZ set_tz(tz);
      system_snapshot().TimeZone(&dst_status,
                                 &standard_offset_seconds,
                                 &daylight_offset_seconds,
                                 &standard_name,
                                 &daylight_name);
    }

    EXPECT_EQ(dst_status != SystemSnapshot::kDoesNotObserveDaylightSavingTime,
              test_time_zone.observes_dst);
    EXPECT_EQ(standard_offset_seconds,
              test_time_zone.standard_offset_hours * 60 * 60);
    EXPECT_EQ(daylight_offset_seconds,
              test_time_zone.daylight_offset_hours * 60 * 60);
    if (test_time_zone.standard_name) {
      EXPECT_EQ(standard_name, test_time_zone.standard_name);
    }
    if (test_time_zone.daylight_name) {
      EXPECT_EQ(daylight_name, test_time_zone.daylight_name);
    }
  }
}

}  // namespace
}  // namespace test
}  // namespace crashpad
