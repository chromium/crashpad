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

#import <XCTest/XCTest.h>

#include <objc/runtime.h>
#import "Service/Sources/EDOClientService.h"
#import "test/ios/host/cptest_shared_object.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CPTestTestCase : XCTestCase {
  XCUIApplication* _app;
}

@end

@implementation CPTestTestCase

- (void)handleCrashUnderSymbol:(id)arg1 {
  // For now, do nothing.  In the future this can be something testable.
}

+ (void)setUp {
  // Swizzle away the handleCrashUnderSymbol callback.  Without this, any time
  // the host app is intentionally crashed, the test is immediately failed.
  SEL originalSelector = NSSelectorFromString(@"handleCrashUnderSymbol:");
  SEL swizzledSelector = @selector(handleCrashUnderSymbol:);

  Method originalMethod = class_getInstanceMethod(
      objc_getClass("XCUIApplicationImpl"), originalSelector);
  Method swizzledMethod =
      class_getInstanceMethod([self class], swizzledSelector);

  method_exchangeImplementations(originalMethod, swizzledMethod);

  // Override EDO default error handler.  Without this, the default EDO error
  // handler will throw an error and fail the test.
  EDOSetClientErrorHandler(^(NSError* error) {
      // Do nothing.
  });
}

- (void)setUp {
  _app = [[XCUIApplication alloc] init];
  [_app launch];
}

- (void)testEDO {
  CPTestSharedObject* rootObject = [EDOClientService rootObjectWithPort:12345];
  NSString* result = [rootObject testEDO];
  XCTAssertEqualObjects(result, @"crashpad");
}

- (void)testSegv {
  XCTAssertTrue(_app.state == XCUIApplicationStateRunningForeground);

  // Crash the app.
  CPTestSharedObject* rootObject = [EDOClientService rootObjectWithPort:12345];
  [rootObject crashSegv];

  // Confirm the app is not running.
  XCTAssertTrue([_app waitForState:XCUIApplicationStateNotRunning timeout:15]);
  XCTAssertTrue(_app.state == XCUIApplicationStateNotRunning);

  // TODO: Query the app for crash data
  [_app launch];
  XCTAssertTrue(_app.state == XCUIApplicationStateRunningForeground);
  rootObject = [EDOClientService rootObjectWithPort:12345];
  XCTAssert([rootObject verifyLastException:SIGHUP]);
}

- (void)testKillAbort {
  XCTAssertTrue(_app.state == XCUIApplicationStateRunningForeground);

  // Crash the app.
  CPTestSharedObject* rootObject = [EDOClientService rootObjectWithPort:12345];
  [rootObject crashKillAbort];

  // Confirm the app is not running.
  XCTAssertTrue([_app waitForState:XCUIApplicationStateNotRunning timeout:15]);
  XCTAssertTrue(_app.state == XCUIApplicationStateNotRunning);

  // TODO: Query the app for crash data
  [_app launch];
  XCTAssertTrue(_app.state == XCUIApplicationStateRunningForeground);
  rootObject = [EDOClientService rootObjectWithPort:12345];
  XCTAssert([rootObject verifyLastException:SIGABRT]);
}

- (void)testTrap {
  XCTAssertTrue(_app.state == XCUIApplicationStateRunningForeground);

  // Crash the app.
  CPTestSharedObject* rootObject = [EDOClientService rootObjectWithPort:12345];
  [rootObject crashTrap];

  // Confirm the app is not running.
  XCTAssertTrue([_app waitForState:XCUIApplicationStateNotRunning timeout:15]);
  XCTAssertTrue(_app.state == XCUIApplicationStateNotRunning);

  // TODO: Query the app for crash data
  [_app launch];
  XCTAssertTrue(_app.state == XCUIApplicationStateRunningForeground);
  rootObject = [EDOClientService rootObjectWithPort:12345];
  XCTAssert([rootObject verifyLastException:SIGINT]);
}

- (void)testAbort {
  XCTAssertTrue(_app.state == XCUIApplicationStateRunningForeground);

  // Crash the app.
  CPTestSharedObject* rootObject = [EDOClientService rootObjectWithPort:12345];
  [rootObject crashAbort];

  // Confirm the app is not running.
  XCTAssertTrue([_app waitForState:XCUIApplicationStateNotRunning timeout:15]);
  XCTAssertTrue(_app.state == XCUIApplicationStateNotRunning);

  // TODO: Query the app for crash data
  [_app launch];
  XCTAssertTrue(_app.state == XCUIApplicationStateRunningForeground);
  rootObject = [EDOClientService rootObjectWithPort:12345];
  XCTAssert([rootObject verifyLastException:SIGABRT]);
}

- (void)testBadAccess {
  XCTAssertTrue(_app.state == XCUIApplicationStateRunningForeground);

  // Crash the app.
  CPTestSharedObject* rootObject = [EDOClientService rootObjectWithPort:12345];
  [rootObject crashBadAccess];

  // Confirm the app is not running.
  XCTAssertTrue([_app waitForState:XCUIApplicationStateNotRunning timeout:15]);
  XCTAssertTrue(_app.state == XCUIApplicationStateNotRunning);

  [_app launch];
  XCTAssertTrue(_app.state == XCUIApplicationStateRunningForeground);
  rootObject = [EDOClientService rootObjectWithPort:12345];
  XCTAssert([rootObject verifyLastException:SIGHUP]);
}

- (void)testException {
  XCTAssertTrue(_app.state == XCUIApplicationStateRunningForeground);

  // Crash the app.
  CPTestSharedObject* rootObject = [EDOClientService rootObjectWithPort:12345];
  [rootObject crashException];

  // Confirm the app is not running.
  XCTAssertTrue([_app waitForState:XCUIApplicationStateNotRunning timeout:15]);
  XCTAssertTrue(_app.state == XCUIApplicationStateNotRunning);

  // TODO: Query the app for crash data
  [_app launch];
  XCTAssertTrue(_app.state == XCUIApplicationStateRunningForeground);
  rootObject = [EDOClientService rootObjectWithPort:12345];
  XCTAssert([rootObject verifyLastException:SIGABRT]);
}

- (void)testNSException {
  XCTAssertTrue(_app.state == XCUIApplicationStateRunningForeground);

  // Crash the app.
  CPTestSharedObject* rootObject = [EDOClientService rootObjectWithPort:12345];
  [rootObject crashNSException];

  // Confirm the app is not running.
  XCTAssertTrue([_app waitForState:XCUIApplicationStateNotRunning timeout:15]);
  XCTAssertTrue(_app.state == XCUIApplicationStateNotRunning);

  // TODO: Query the app for crash data
  [_app launch];
  XCTAssertTrue(_app.state == XCUIApplicationStateRunningForeground);
  rootObject = [EDOClientService rootObjectWithPort:12345];
  XCTAssert([rootObject verifyLastException:SIGABRT]);
}

- (void)testCrashUnreocgnizedSelectorAfterDelay {
  XCTAssertTrue(_app.state == XCUIApplicationStateRunningForeground);

  // Crash the app.
  CPTestSharedObject* rootObject = [EDOClientService rootObjectWithPort:12345];
  [rootObject crashUnreocgnizedSelectorAfterDelay];

  // Confirm the app is not running.
  XCTAssertTrue([_app waitForState:XCUIApplicationStateNotRunning timeout:15]);
  XCTAssertTrue(_app.state == XCUIApplicationStateNotRunning);

  // TODO: Query the app for crash data
  [_app launch];
  XCTAssertTrue(_app.state == XCUIApplicationStateRunningForeground);
  rootObject = [EDOClientService rootObjectWithPort:12345];
  XCTAssert([rootObject verifyLastException:SIGABRT]);
}

- (void)testCatchUIGestureEnvironmentNSException {
  XCTAssertTrue(_app.state == XCUIApplicationStateRunningForeground);

  // Tap the button with the string UIGestureEnvironmentException.
  [_app.buttons[@"UIGestureEnvironmentException"] tap];

  // Confirm the app is not running.
  XCTAssertTrue([_app waitForState:XCUIApplicationStateNotRunning timeout:15]);
  XCTAssertTrue(_app.state == XCUIApplicationStateNotRunning);

  // TODO: Query the app for crash data
  [_app launch];
  XCTAssertTrue(_app.state == XCUIApplicationStateRunningForeground);
  CPTestSharedObject* rootObject = [EDOClientService rootObjectWithPort:12345];
  rootObject = [EDOClientService rootObjectWithPort:12345];
  XCTAssert([rootObject verifyLastException:SIGABRT]);
}

- (void)testCatchNSException {
  XCTAssertTrue(_app.state == XCUIApplicationStateRunningForeground);

  // The app should not crash
  CPTestSharedObject* rootObject = [EDOClientService rootObjectWithPort:12345];
  [rootObject catchNSException];

  XCTAssertTrue(_app.state == XCUIApplicationStateRunningForeground);
  // verify no dumps.
}

- (void)testRecursion {
  // TODO(justincohen): Crashpad iOS does not currently support stack type
  // crashes.
  return;

  XCTAssertTrue(_app.state == XCUIApplicationStateRunningForeground);

  // Crash the app.
  CPTestSharedObject* rootObject = [EDOClientService rootObjectWithPort:12345];
  [rootObject crashRecursion];

  // Confirm the app is not running.
  XCTAssertTrue([_app waitForState:XCUIApplicationStateNotRunning timeout:15]);
  XCTAssertTrue(_app.state == XCUIApplicationStateNotRunning);

  // TODO: Query the app for crash data
  [_app launch];
  XCTAssertTrue(_app.state == XCUIApplicationStateRunningForeground);
  rootObject = [EDOClientService rootObjectWithPort:12345];
  XCTAssert([rootObject verifyLastException:SIGABRT]);
}

@end
