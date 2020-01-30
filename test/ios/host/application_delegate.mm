// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "test/ios/host/application_delegate.h"

#import "test/ios/host/crash_view_controller.h"
#import "test/ios/host/edo_placeholder.h"
#import "third_party/edo/src/Service/Sources/EDOHostNamingService.h"
#import "third_party/edo/src/Service/Sources/EDOHostService.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ApplicationDelegate
@synthesize window = _window;

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
  [self.window makeKeyAndVisible];
  self.window.backgroundColor = UIColor.greenColor;

  CrashViewController* controller = [[CrashViewController alloc] init];
  self.window.rootViewController = controller;

  // Startup EDO.
  [EDOHostService serviceWithPort:12345
                       rootObject:[[EDOPlaceholder alloc] init]
                            queue:dispatch_get_main_queue()];
  [EDOHostNamingService.sharedService start];

  return YES;
}

@end

@implementation EDOPlaceholder
- (NSString*)testEDO {
  return @"crashpad";
}
@end
