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

#import "test/ios/host/application_delegate.h"

#import "Service/Sources/EDOHostNamingService.h"
#import "Service/Sources/EDOHostService.h"
#include "client/crashpad_client.h"
#import "test/ios/host/cptest_shared_object.h"
#import "test/ios/host/crash_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ApplicationDelegate
@synthesize window = _window;

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  // Start up crashpad.
  crashpad::CrashpadClient client;
  client.StartCrashpadInProcessHandler();

  self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
  [self.window makeKeyAndVisible];
  self.window.backgroundColor = UIColor.greenColor;

  CrashViewController* controller = [[CrashViewController alloc] init];
  self.window.rootViewController = controller;

  // Start up EDO.
  [EDOHostService serviceWithPort:12345
                       rootObject:[[CPTestSharedObject alloc] init]
                            queue:dispatch_get_main_queue()];
  return YES;
}

@end

@implementation CPTestSharedObject
- (NSString*)testEDO {
  return @"crashpad";
}

- (void)crashBadAccess {
  strcpy(0, "bla");
}

- (void)crashKillAbort {
  kill(getpid(), SIGABRT);
}

- (void)crashSegv {
  long zero = 0;
  *(long*)zero = 0xC045004d;
}

- (void)crashTrap {
  __builtin_trap();
}

- (void)crashAbort {
  abort();
}

@end
