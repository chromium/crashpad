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

#include "test/gtest_runner_ios.h"

#import <UIKit/UIKit.h>

#include "gtest/gtest.h"

@interface UIApplication (Testing)
- (void)_terminateWithStatus:(int)status;
@end

@interface CrashpadUnitTestDelegate : NSObject
@property(nonatomic, readwrite, strong) UIWindow* window;
- (void)runTests;
@end

@implementation CrashpadUnitTestDelegate

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
  self.window.backgroundColor = UIColor.whiteColor;
  [self.window makeKeyAndVisible];

  UIViewController* controller = [[UIViewController alloc] init];
  [self.window setRootViewController:controller];

  // Add a label with the app name.
  UILabel* label = [[UILabel alloc] initWithFrame:controller.view.bounds];
  label.text = [[NSProcessInfo processInfo] processName];
  label.textAlignment = NSTextAlignmentCenter;
  label.textColor = UIColor.blackColor;
  [controller.view addSubview:label];

  // Queue up the test run.
  [self performSelector:@selector(runTests) withObject:nil afterDelay:0.1];

  return YES;
}

- (void)runTests {
  int exitStatus = RUN_ALL_TESTS();

  // If a test app is too fast, it will exit before Instruments has has a
  // a chance to initialize and no test results will be seen.
  // TODO(crbug.com/137010): Figure out how much time is actually needed, and
  // sleep only to make sure that much time has elapsed since launch.
  [NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow:2.0]];
  self.window = nil;

  // Use the hidden selector to try and cleanly take down the app (otherwise
  // things can think the app crashed even on a zero exit status).
  UIApplication* application = [UIApplication sharedApplication];
  [application _terminateWithStatus:exitStatus];

  exit(exitStatus);
}

@end

void IOSLaunchApplicationAndRunTests(int argc, char* argv[]) {
  @autoreleasepool {
    int exit_status =
        UIApplicationMain(argc, argv, nil, @"CrashpadUnitTestDelegate");
    exit(exit_status);
  }
}
