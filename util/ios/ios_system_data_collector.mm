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

#include "util/ios/ios_system_data_collector.h"

namespace crashpad {

IOSSystemDataCollector::IOSSystemDataCollector() {
  char str[256];
  size_t size = sizeof(str);
  sysctlbyname("kern.osversion", str, &size, NULL, 0);
  build_ = std::string(str, size - 1);

  NSOperatingSystemVersion version =
      [[NSProcessInfo processInfo] operatingSystemVersion];
  major_version_ = version.majorVersion;
  minor_version_ = version.minorVersion;
  patch_version_ = version.patchVersion;
  processor_count_ = [[NSProcessInfo processInfo] processorCount];

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
      IOSSystemDataCollector::SystemTimeZoneDidChangeNotificationHandler,
      (CFStringRef)NSSystemTimeZoneDidChangeNotification,
      NULL,
      CFNotificationSuspensionBehaviorDeliverImmediately);

  // Orientation.
  orientation_ = [[UIDevice currentDevice] orientation];
  CFNotificationCenterAddObserver(
      CFNotificationCenterGetLocalCenter(),
      this,
      IOSSystemDataCollector::OrientationDidChangeNotificationHandler,
      (CFStringRef)UIDeviceOrientationDidChangeNotification,
      NULL,
      CFNotificationSuspensionBehaviorDeliverImmediately);

  // Application state.
  application_state_ = [[UIApplication sharedApplication] applicationState];
  CFNotificationCenterAddObserver(
      CFNotificationCenterGetLocalCenter(),
      this,
      IOSSystemDataCollector::ApplicationStateChangeNotificationHandler,
      (CFStringRef)UIApplicationDidEnterBackgroundNotification,
      NULL,
      CFNotificationSuspensionBehaviorDeliverImmediately);
  CFNotificationCenterAddObserver(
      CFNotificationCenterGetLocalCenter(),
      this,
      IOSSystemDataCollector::ApplicationStateChangeNotificationHandler,
      (CFStringRef)UIApplicationDidBecomeActiveNotification,
      NULL,
      CFNotificationSuspensionBehaviorDeliverImmediately);
}

void IOSSystemDataCollector::SystemTimeZoneDidChangeNotificationHandler(
    CFNotificationCenterRef center,
    void* observer,
    CFStringRef name,
    const void* object,
    CFDictionaryRef userInfo) {
  (static_cast<IOSSystemDataCollector*>(observer))
      ->SystemTimeZoneDidChangeNotification();
}

void IOSSystemDataCollector::SystemTimeZoneDidChangeNotification() {
  NSTimeZone* time_zone = NSTimeZone.localTimeZone;
  NSDate* transition = [time_zone nextDaylightSavingTimeTransitionAfterDate:[NSDate date]];
  if (transition == nil) {
    dst_status_ = SystemSnapshot::kDoesNotObserveDaylightSavingTime;
    daylight_name_ = [[time_zone abbreviation] UTF8String];
    standard_name_ = daylight_name_;
  } else if (time_zone.isDaylightSavingTime) {
    dst_status_ = SystemSnapshot::kObservingDaylightSavingTime;
    daylight_offset_seconds_ = [time_zone secondsFromGMT];
    standard_offset_seconds_ = [time_zone secondsFromGMTForDate:transition];
    daylight_name_ = [[time_zone abbreviation] UTF8String];
    standard_name_ = [[time_zone abbreviationForDate:transition] UTF8String];
  } else {
    dst_status_ = SystemSnapshot::kObservingStandardTime;
    standard_name_ = [[time_zone abbreviation] UTF8String];
    daylight_name_ = [[time_zone abbreviationForDate:transition] UTF8String];
    standard_offset_seconds_ = [time_zone secondsFromGMT];
    daylight_offset_seconds_ = [time_zone secondsFromGMTForDate:transition];
  }
}

void IOSSystemDataCollector::ApplicationStateChangeNotificationHandler(
    CFNotificationCenterRef center,
    void* observer,
    CFStringRef name,
    const void* object,
    CFDictionaryRef userInfo) {
  (static_cast<IOSSystemDataCollector*>(observer))
      ->ApplicationStateDidChangeNotification();
}

void IOSSystemDataCollector::ApplicationStateDidChangeNotification() {
  application_state_ = [[UIApplication sharedApplication] applicationState];
}

void IOSSystemDataCollector::OrientationDidChangeNotificationHandler(
    CFNotificationCenterRef center,
    void* observer,
    CFStringRef name,
    const void* object,
    CFDictionaryRef userInfo) {
  (static_cast<IOSSystemDataCollector*>(observer))->OrientationDidChangeNotification();
}

void IOSSystemDataCollector::OrientationDidChangeNotification() {
  orientation_ = [[UIDevice currentDevice] orientation];
}

IOSSystemDataCollector::~IOSSystemDataCollector() {
  CFNotificationCenterRemoveEveryObserver(CFNotificationCenterGetLocalCenter(),
                                          this);
}

}  // namespace crashpad
