// Copyright 2020 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_UTIL_IOS_IOS_SYSTEM_DATA_COLLECTOR_H_
#define CRASHPAD_UTIL_IOS_IOS_SYSTEM_DATA_COLLECTOR_H_

#import <CoreFoundation/CoreFoundation.h>

#include "snapshot/system_snapshot.h"

namespace crashpad {

//! \brief Used to collect system level data before a crash occurs.
class IOSSystemDataCollector {
 public:
  IOSSystemDataCollector();
  ~IOSSystemDataCollector();

  int MajorVersion() const { return major_version_; }
  int MinorVersion() const { return minor_version_; }
  int PatchVersion() const { return patch_version_; }
  std::string Build() const { return build_; }
  std::string Machine() const { return machine_; }
  int ProcessorCount() const { return processor_count_; }
  SystemSnapshot::DaylightSavingTimeStatus DSTStatus() const {
    return dst_status_;
  }
  int StandardOffsetSeconds() const { return standard_offset_seconds_; }
  int DaylightOffsetSeconds() const { return daylight_offset_seconds_; }
  std::string StandardName() const { return standard_name_; }
  std::string DaylightName() const { return daylight_name_; }

  // Currently unused by minidump.
  int Orientation() const { return orientation_; }
  int ApplicationState() const { return application_state_; }

 private:
  // Notification handlers.
  static void OrientationDidChangeNotificationHandler(
      CFNotificationCenterRef center,
      void* observer,
      CFStringRef name,
      const void* object,
      CFDictionaryRef userInfo);
  void OrientationDidChangeNotification();

  static void ApplicationStateChangeNotificationHandler(
      CFNotificationCenterRef center,
      void* observer,
      CFStringRef name,
      const void* object,
      CFDictionaryRef userInfo);
  void ApplicationStateDidChangeNotification();

  static void SystemTimeZoneDidChangeNotificationHandler(
      CFNotificationCenterRef center,
      void* observer,
      CFStringRef name,
      const void* object,
      CFDictionaryRef userInfo);
  void SystemTimeZoneDidChangeNotification();

  int major_version_;
  int minor_version_;
  int patch_version_;
  std::string build_;
  std::string machine_;
  int orientation_;
  int application_state_;
  int processor_count_;
  SystemSnapshot::DaylightSavingTimeStatus dst_status_;
  int standard_offset_seconds_;
  int daylight_offset_seconds_;
  std::string standard_name_;
  std::string daylight_name_;
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_IOS_IOS_SYSTEM_DATA_COLLECTOR_H_
