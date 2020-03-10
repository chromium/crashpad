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

#ifndef CRASHPAD_UTIL_IOS_IOS_SYSTEM_DATA_H_
#define CRASHPAD_UTIL_IOS_IOS_SYSTEM_DATA_H_

#import <CoreFoundation/CoreFoundation.h>

#include "snapshot/system_snapshot.h"

namespace crashpad {

//! \brief A class to capture data easilly found in Foundation.  To be called
//! and cached before a crash.
class IOSSystemData {
 public:
  IOSSystemData();
  ~IOSSystemData();
  int majorVersion_;
  int minorVersion_;
  int patchVersion_;
  std::string build_;
  std::string machine_;
  int orientation_;
  int applicationState_;
  int pageSize_;
  int processorCount_;
  SystemSnapshot::DaylightSavingTimeStatus dstStatus_;
  int standard_offset_seconds_;
  int daylight_offset_seconds_;
  std::string standard_name_;
  std::string daylight_name_;

 private:
  static void OrientationDidChangeNotificationHandler(
      CFNotificationCenterRef center,
      void* observer,
      CFStringRef name,
      const void* object,
      CFDictionaryRef userInfo);
  static void ApplicationStateChangeNotificationHandler(
      CFNotificationCenterRef center,
      void* observer,
      CFStringRef name,
      const void* object,
      CFDictionaryRef userInfo);
  static void SystemTimeZoneDidChangeNotificationHandler(
      CFNotificationCenterRef center,
      void* observer,
      CFStringRef name,
      const void* object,
      CFDictionaryRef userInfo);

  void OrientationDidChangeNotification();
  void ApplicationStateDidChangeNotification();
  void SystemTimeZoneDidChangeNotification();
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_IOS_IOS_SYSTEM_DATA_H_
