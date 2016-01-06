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

#include "util/mac/launchd.h"

#import <Foundation/Foundation.h>
#include <launch.h>
#include <stdint.h>
#include <string.h>

#include <cmath>
#include <limits>

#include "base/mac/scoped_launch_data.h"
#include "base/macros.h"
#include "gtest/gtest.h"
#include "util/stdlib/objc.h"

namespace crashpad {
namespace test {
namespace {

TEST(Launchd, CFPropertyToLaunchData_Integer) {
  @autoreleasepool {
    base::mac::ScopedLaunchData launch_data;

    NSNumber* integer_nses[] = {
      @0,
      @1,
      @-1,
      @0x7f,
      @0x80,
      @0xff,
      @0x0100,
      @0x7fff,
      @0x8000,
      @0xffff,
      @0x00010000,
      @0x7fffffff,
      @0x80000000,
      @0xffffffff,
      @0x1000000000000000,
      @0x7fffffffffffffff,
      @0x8000000000000000,
      @0xffffffffffffffff,
      @0x0123456789abcdef,
      @0xfedcba9876543210,
    };

    for (size_t index = 0; index < arraysize(integer_nses); ++index) {
      NSNumber* integer_ns = integer_nses[index];
      launch_data.reset(CFPropertyToLaunchData(integer_ns));
      ASSERT_TRUE(launch_data.get());
      ASSERT_EQ(LAUNCH_DATA_INTEGER, LaunchDataGetType(launch_data.get()));
      EXPECT_EQ([integer_ns longLongValue],
                LaunchDataGetInteger(launch_data.get()))
          << "index " << index;
    }
  }
}

TEST(Launchd, CFPropertyToLaunchData_FloatingPoint) {
  @autoreleasepool {
    base::mac::ScopedLaunchData launch_data;

    NSNumber* double_nses[] = {
      @0.0,
      @1.0,
      @-1.0,
      [NSNumber numberWithFloat:std::numeric_limits<float>::min()],
      [NSNumber numberWithFloat:std::numeric_limits<float>::max()],
      [NSNumber numberWithDouble:std::numeric_limits<double>::min()],
      [NSNumber numberWithDouble:std::numeric_limits<double>::max()],
      @3.1415926535897932,
      [NSNumber numberWithDouble:std::numeric_limits<double>::infinity()],
      [NSNumber numberWithDouble:std::numeric_limits<double>::quiet_NaN()],
      [NSNumber numberWithDouble:std::numeric_limits<double>::signaling_NaN()],
    };

    for (size_t index = 0; index < arraysize(double_nses); ++index) {
      NSNumber* double_ns = double_nses[index];
      launch_data.reset(CFPropertyToLaunchData(double_ns));
      ASSERT_TRUE(launch_data.get());
      ASSERT_EQ(LAUNCH_DATA_REAL, LaunchDataGetType(launch_data.get()));
      double expected_double_value = [double_ns doubleValue];
      double observed_double_value = LaunchDataGetReal(launch_data.get());
      bool expected_is_nan = std::isnan(expected_double_value);
      EXPECT_EQ(expected_is_nan, std::isnan(observed_double_value));
      if (!expected_is_nan) {
        EXPECT_DOUBLE_EQ(expected_double_value, observed_double_value)
            << "index " << index;
      }
    }
  }
}

TEST(Launchd, CFPropertyToLaunchData_Boolean) {
  @autoreleasepool {
    base::mac::ScopedLaunchData launch_data;

    NSNumber* bool_nses[] = {
      @NO,
      @YES,
    };

    for (size_t index = 0; index < arraysize(bool_nses); ++index) {
      NSNumber* bool_ns = bool_nses[index];
      launch_data.reset(CFPropertyToLaunchData(bool_ns));
      ASSERT_TRUE(launch_data.get());
      ASSERT_EQ(LAUNCH_DATA_BOOL, LaunchDataGetType(launch_data.get()));
      if ([bool_ns boolValue]) {
        EXPECT_TRUE(LaunchDataGetBool(launch_data.get()));
      } else {
        EXPECT_FALSE(LaunchDataGetBool(launch_data.get()));
      }
    }
  }
}

TEST(Launchd, CFPropertyToLaunchData_String) {
  @autoreleasepool {
    base::mac::ScopedLaunchData launch_data;

    NSString* string_nses[] = {
      @"",
      @"string",
      @"Üñîçø∂é",
    };

    for (size_t index = 0; index < arraysize(string_nses); ++index) {
      NSString* string_ns = string_nses[index];
      launch_data.reset(CFPropertyToLaunchData(string_ns));
      ASSERT_TRUE(launch_data.get());
      ASSERT_EQ(LAUNCH_DATA_STRING, LaunchDataGetType(launch_data.get()));
      EXPECT_STREQ([string_ns UTF8String],
                   LaunchDataGetString(launch_data.get()));
    }
  }
}

TEST(Launchd, CFPropertyToLaunchData_Data) {
  @autoreleasepool {
    base::mac::ScopedLaunchData launch_data;

    const uint8_t data_c[] = {
        1, 2, 3, 4, 5, 6, 7, 8, 0, 0, 0, 0, 0, 0, 0, 0, 9, 8, 7, 6, 5, 4, 3, 2};
    NSData* data_ns = [NSData dataWithBytes:data_c length:sizeof(data_c)];
    launch_data.reset(CFPropertyToLaunchData(data_ns));
    ASSERT_TRUE(launch_data.get());
    ASSERT_EQ(LAUNCH_DATA_OPAQUE, LaunchDataGetType(launch_data.get()));
    EXPECT_EQ(sizeof(data_c), LaunchDataGetOpaqueSize(launch_data.get()));
    EXPECT_EQ(
        0,
        memcmp(LaunchDataGetOpaque(launch_data.get()), data_c, sizeof(data_c)));
  }
}

TEST(Launchd, CFPropertyToLaunchData_Dictionary) {
  @autoreleasepool {
    base::mac::ScopedLaunchData launch_data;

    NSDictionary* dictionary_ns = @{
      @"key" : @"value",
    };

    launch_data.reset(CFPropertyToLaunchData(dictionary_ns));
    ASSERT_TRUE(launch_data.get());
    ASSERT_EQ(LAUNCH_DATA_DICTIONARY, LaunchDataGetType(launch_data.get()));
    EXPECT_EQ([dictionary_ns count], LaunchDataDictGetCount(launch_data.get()));

    launch_data_t launch_lookup_data =
        LaunchDataDictLookup(launch_data.get(), "key");
    ASSERT_TRUE(launch_lookup_data);
    ASSERT_EQ(LAUNCH_DATA_STRING, LaunchDataGetType(launch_lookup_data));
    EXPECT_STREQ("value", LaunchDataGetString(launch_lookup_data));
  }
}

TEST(Launchd, CFPropertyToLaunchData_Array) {
  @autoreleasepool {
    base::mac::ScopedLaunchData launch_data;

    NSArray* array_ns = @[ @"element_1", @"element_2", ];

    launch_data.reset(CFPropertyToLaunchData(array_ns));
    ASSERT_TRUE(launch_data.get());
    ASSERT_EQ(LAUNCH_DATA_ARRAY, LaunchDataGetType(launch_data.get()));
    EXPECT_EQ([array_ns count], LaunchDataArrayGetCount(launch_data.get()));

    launch_data_t launch_lookup_data =
        LaunchDataArrayGetIndex(launch_data.get(), 0);
    ASSERT_TRUE(launch_lookup_data);
    ASSERT_EQ(LAUNCH_DATA_STRING, LaunchDataGetType(launch_lookup_data));
    EXPECT_STREQ("element_1", LaunchDataGetString(launch_lookup_data));

    launch_lookup_data = LaunchDataArrayGetIndex(launch_data.get(), 1);
    ASSERT_TRUE(launch_lookup_data);
    ASSERT_EQ(LAUNCH_DATA_STRING, LaunchDataGetType(launch_lookup_data));
    EXPECT_STREQ("element_2", LaunchDataGetString(launch_lookup_data));
  }
}

TEST(Launchd, CFPropertyToLaunchData_NSDate) {
  // Test that NSDate conversions fail as advertised. There’s no type for
  // storing date values in a launch_data_t.

  @autoreleasepool {
    base::mac::ScopedLaunchData launch_data;

    NSDate* date = [NSDate date];
    launch_data.reset(CFPropertyToLaunchData(date));
    EXPECT_FALSE(launch_data.get());

    NSDictionary* date_dictionary = @{
      @"key" : @"value",
      @"date" : date,
    };
    launch_data.reset(CFPropertyToLaunchData(date_dictionary));
    EXPECT_FALSE(launch_data.get());

    NSArray* date_array = @[ @"string_1", date, @"string_2", ];
    launch_data.reset(CFPropertyToLaunchData(date_array));
    EXPECT_FALSE(launch_data.get());
  }
}

TEST(Launchd, CFPropertyToLaunchData_RealWorldJobDictionary) {
  @autoreleasepool {
    base::mac::ScopedLaunchData launch_data;

    NSDictionary* job_dictionary = @{
      @LAUNCH_JOBKEY_LABEL : @"com.example.job.rebooter",
      @LAUNCH_JOBKEY_ONDEMAND : @YES,
      @LAUNCH_JOBKEY_PROGRAMARGUMENTS :
          @[ @"/bin/bash", @"-c", @"/sbin/reboot", ],
      @LAUNCH_JOBKEY_MACHSERVICES : @{
        @"com.example.service.rebooter" : @YES,
      },
    };

    launch_data.reset(CFPropertyToLaunchData(job_dictionary));
    ASSERT_TRUE(launch_data.get());
    ASSERT_EQ(LAUNCH_DATA_DICTIONARY, LaunchDataGetType(launch_data.get()));
    EXPECT_EQ(4u, LaunchDataDictGetCount(launch_data.get()));

    launch_data_t launch_lookup_data =
        LaunchDataDictLookup(launch_data.get(), LAUNCH_JOBKEY_LABEL);
    ASSERT_TRUE(launch_lookup_data);
    ASSERT_EQ(LAUNCH_DATA_STRING, LaunchDataGetType(launch_lookup_data));
    EXPECT_STREQ("com.example.job.rebooter",
                 LaunchDataGetString(launch_lookup_data));

    launch_lookup_data =
        LaunchDataDictLookup(launch_data.get(), LAUNCH_JOBKEY_ONDEMAND);
    ASSERT_TRUE(launch_lookup_data);
    ASSERT_EQ(LAUNCH_DATA_BOOL, LaunchDataGetType(launch_lookup_data));
    EXPECT_TRUE(LaunchDataGetBool(launch_lookup_data));

    launch_lookup_data =
        LaunchDataDictLookup(launch_data.get(), LAUNCH_JOBKEY_PROGRAMARGUMENTS);
    ASSERT_TRUE(launch_lookup_data);
    ASSERT_EQ(LAUNCH_DATA_ARRAY, LaunchDataGetType(launch_lookup_data));
    EXPECT_EQ(3u, LaunchDataArrayGetCount(launch_lookup_data));

    launch_data_t launch_sublookup_data =
        LaunchDataArrayGetIndex(launch_lookup_data, 0);
    ASSERT_TRUE(launch_sublookup_data);
    ASSERT_EQ(LAUNCH_DATA_STRING, LaunchDataGetType(launch_sublookup_data));
    EXPECT_STREQ("/bin/bash", LaunchDataGetString(launch_sublookup_data));

    launch_sublookup_data = LaunchDataArrayGetIndex(launch_lookup_data, 1);
    ASSERT_TRUE(launch_sublookup_data);
    ASSERT_EQ(LAUNCH_DATA_STRING, LaunchDataGetType(launch_sublookup_data));
    EXPECT_STREQ("-c", LaunchDataGetString(launch_sublookup_data));

    launch_sublookup_data = LaunchDataArrayGetIndex(launch_lookup_data, 2);
    ASSERT_TRUE(launch_sublookup_data);
    ASSERT_EQ(LAUNCH_DATA_STRING, LaunchDataGetType(launch_sublookup_data));
    EXPECT_STREQ("/sbin/reboot", LaunchDataGetString(launch_sublookup_data));

    launch_lookup_data =
        LaunchDataDictLookup(launch_data.get(), LAUNCH_JOBKEY_MACHSERVICES);
    ASSERT_TRUE(launch_lookup_data);
    ASSERT_EQ(LAUNCH_DATA_DICTIONARY, LaunchDataGetType(launch_lookup_data));
    EXPECT_EQ(1u, LaunchDataDictGetCount(launch_lookup_data));

    launch_sublookup_data = LaunchDataDictLookup(
        launch_lookup_data, "com.example.service.rebooter");
    ASSERT_TRUE(launch_sublookup_data);
    ASSERT_EQ(LAUNCH_DATA_BOOL, LaunchDataGetType(launch_sublookup_data));
    EXPECT_TRUE(LaunchDataGetBool(launch_sublookup_data));
  }
}

}  // namespace
}  // namespace test
}  // namespace crashpad
