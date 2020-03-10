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

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#include <sys/sysctl.h>
#import <sys/utsname.h>
#import <string>

#include "util/ios/ios_system_data.h"

namespace crashpad {

IOSSystemData::IOSSystemData() {
  // sysctlbyname requires libc.
  char str[256];
  size_t size = sizeof(str);
  sysctlbyname("kern.osversion", str, &size, NULL, 0);
  build_ = std::string(str, size - 1);

  NSOperatingSystemVersion version =
      [[NSProcessInfo processInfo] operatingSystemVersion];
  majorVersion_ = version.majorVersion;
  minorVersion_ = version.minorVersion;
  patchVersion_ = version.patchVersion;
  processorCount_ = [[NSProcessInfo processInfo] processorCount];

#if !TARGET_IPHONE_SIMULATOR
  utsname uts;
  if (uname(&uts) == 0) {
    machine_ = uts.machine;
  }
#else
  switch (UI_USER_INTERFACE_IDIOM()) {
    case UIUserInterfaceIdiomPhone:
      machine_ = "iOS Simulator (iPhone)";
      break;
    case UIUserInterfaceIdiomPad:
      machine_ = "iOS Simulator (iPad)";
      break;
    default:
      machine_ = "iOS Simulator (Unknown)";
      break;
  }
#endif

  // Timezone.
  SystemTimeZoneDidChangeNotification();
  CFNotificationCenterAddObserver(
      CFNotificationCenterGetLocalCenter(),
      this,
      IOSSystemData::SystemTimeZoneDidChangeNotificationHandler,
      (CFStringRef)NSSystemTimeZoneDidChangeNotification,
      NULL,
      CFNotificationSuspensionBehaviorDeliverImmediately);

  // Orientation.
  orientation_ = [[UIDevice currentDevice] orientation];
  CFNotificationCenterAddObserver(
      CFNotificationCenterGetLocalCenter(),
      this,
      IOSSystemData::OrientationDidChangeNotificationHandler,
      (CFStringRef)UIDeviceOrientationDidChangeNotification,
      NULL,
      CFNotificationSuspensionBehaviorDeliverImmediately);

  // Application state.
  applicationState_ = [[UIApplication sharedApplication] applicationState];
  CFNotificationCenterAddObserver(
      CFNotificationCenterGetLocalCenter(),
      this,
      IOSSystemData::ApplicationStateChangeNotificationHandler,
      (CFStringRef)UIApplicationDidEnterBackgroundNotification,
      NULL,
      CFNotificationSuspensionBehaviorDeliverImmediately);

  CFNotificationCenterAddObserver(
      CFNotificationCenterGetLocalCenter(),
      this,
      IOSSystemData::ApplicationStateChangeNotificationHandler,
      (CFStringRef)UIApplicationDidBecomeActiveNotification,
      NULL,
      CFNotificationSuspensionBehaviorDeliverImmediately);
}

void IOSSystemData::SystemTimeZoneDidChangeNotificationHandler(
    CFNotificationCenterRef center,
    void* observer,
    CFStringRef name,
    const void* object,
    CFDictionaryRef userInfo) {
  (static_cast<IOSSystemData*>(observer))
      ->SystemTimeZoneDidChangeNotification();
}

void IOSSystemData::SystemTimeZoneDidChangeNotification() {
  NSTimeZone* timeZone = NSTimeZone.localTimeZone;
  NSDate* now = [NSDate date];
  NSDate* transition = [timeZone nextDaylightSavingTimeTransitionAfterDate:now];
  if (transition == nil) {
    dstStatus_ = SystemSnapshot::kDoesNotObserveDaylightSavingTime;
    daylight_name_ = [[timeZone abbreviation] UTF8String];
    standard_name_ = daylight_name_;
  } else if (timeZone.isDaylightSavingTime) {
    dstStatus_ = SystemSnapshot::kObservingDaylightSavingTime;
    daylight_offset_seconds_ = [timeZone secondsFromGMT];
    standard_offset_seconds_ = [timeZone secondsFromGMTForDate:transition];
    daylight_name_ = [[timeZone abbreviation] UTF8String];
    standard_name_ = [[timeZone abbreviationForDate:transition] UTF8String];
  } else {
    dstStatus_ = SystemSnapshot::kObservingStandardTime;
    standard_name_ = [[timeZone abbreviation] UTF8String];
    daylight_name_ = [[timeZone abbreviationForDate:transition] UTF8String];
    standard_offset_seconds_ = [timeZone secondsFromGMT];
    daylight_offset_seconds_ = [timeZone secondsFromGMTForDate:transition];
  }
}

void IOSSystemData::ApplicationStateChangeNotificationHandler(
    CFNotificationCenterRef center,
    void* observer,
    CFStringRef name,
    const void* object,
    CFDictionaryRef userInfo) {
  (static_cast<IOSSystemData*>(observer))
      ->ApplicationStateDidChangeNotification();
}

void IOSSystemData::ApplicationStateDidChangeNotification() {
  applicationState_ = [[UIApplication sharedApplication] applicationState];
}

void IOSSystemData::OrientationDidChangeNotificationHandler(
    CFNotificationCenterRef center,
    void* observer,
    CFStringRef name,
    const void* object,
    CFDictionaryRef userInfo) {
  (static_cast<IOSSystemData*>(observer))->OrientationDidChangeNotification();
}

void IOSSystemData::OrientationDidChangeNotification() {
  orientation_ = [[UIDevice currentDevice] orientation];
}

IOSSystemData::~IOSSystemData() {
  CFNotificationCenterRemoveEveryObserver(CFNotificationCenterGetLocalCenter(),
                                          this);
}

}  // namespace crashpad
