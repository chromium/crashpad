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
  CPTestSharedObject* _rootObject;
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
  _rootObject = [EDOClientService rootObjectWithPort:12345];
  [_rootObject clearReports];
  XCTAssertEqual([_rootObject pendingReportCount], 0);
  XCTAssertTrue(_app.state == XCUIApplicationStateRunningForeground);
}

- (void)verifyCrashReportSignal:(int)signal {
  // Confirm the app is not running.
  XCTAssertTrue([_app waitForState:XCUIApplicationStateNotRunning timeout:15]);
  XCTAssertTrue(_app.state == XCUIApplicationStateNotRunning);

  // Restart app to get the report signal.
  [_app launch];
  XCTAssertTrue(_app.state == XCUIApplicationStateRunningForeground);
  _rootObject = [EDOClientService rootObjectWithPort:12345];
  XCTAssertEqual([_rootObject pendingReportCount], 1);
  XCTAssertEqual([_rootObject lastException], signal);
}

- (void)testEDO {
  NSString* result = [_rootObject testEDO];
  XCTAssertEqualObjects(result, @"crashpad");
}

- (void)testSegv {
  [_rootObject crashSegv];
#if defined(NDEBUG)
#if TARGET_OS_SIMULATOR
  [self verifyCrashReportSignal:SIGINT];
#else
  [self verifyCrashReportSignal:SIGABRT];
#endif
#else
  [self verifyCrashReportSignal:SIGHUP];
#endif
}

- (void)testKillAbort {
  [_rootObject crashKillAbort];
  [self verifyCrashReportSignal:SIGABRT];
}

- (void)testTrap {
  [_rootObject crashTrap];
#if TARGET_OS_SIMULATOR
  [self verifyCrashReportSignal:SIGINT];
#else
  [self verifyCrashReportSignal:SIGABRT];
#endif
}

- (void)testAbort {
  [_rootObject crashAbort];
  [self verifyCrashReportSignal:SIGABRT];
}

- (void)testBadAccess {
  [_rootObject crashBadAccess];
#if defined(NDEBUG)
#if TARGET_OS_SIMULATOR
  [self verifyCrashReportSignal:SIGINT];
#else
  [self verifyCrashReportSignal:SIGABRT];
#endif
#else
  [self verifyCrashReportSignal:SIGHUP];
#endif
}

- (void)testException {
  [_rootObject crashException];
  [self verifyCrashReportSignal:SIGABRT];
}

- (void)testNSException {
  [_rootObject crashNSException];
  [self verifyCrashReportSignal:SIGABRT];
}

- (void)testCrashUnreocgnizedSelectorAfterDelay {
  [_rootObject crashUnreocgnizedSelectorAfterDelay];
  [self verifyCrashReportSignal:SIGABRT];
}

- (void)testCatchUIGestureEnvironmentNSException {
  // Tap the button with the string UIGestureEnvironmentException.
  [_app.buttons[@"UIGestureEnvironmentException"] tap];
  [self verifyCrashReportSignal:SIGABRT];
}

- (void)testCatchNSException {
  [_rootObject catchNSException];

  // The app should not crash
  XCTAssertTrue(_app.state == XCUIApplicationStateRunningForeground);
  XCTAssertEqual([_rootObject pendingReportCount], 0);
}

- (void)testRecursion {
  [_rootObject crashRecursion];
  [self verifyCrashReportSignal:SIGHUP];
}

- (void)testCrashWithCrashInfoMessage {
  [_rootObject crashWithCrashInfoMessage];
  [self verifyCrashReportSignal:SIGHUP];
  XCTAssertTrue([[_rootObject crashInfoMessage:true]
      isEqualToString:@"dyld: in dlsym()"]);
}

// TODO(justincohen): Figure out how to test this on device.
#if !TARGET_OS_SIMULATOR
- (void)testCrashWithDyldErrorString {
  [_rootObject crashWithDyldErrorString];
  [self verifyCrashReportSignal:SIGABRT];
  XCTAssertTrue([[_rootObject crashInfoMessage:false]
      isEqualToString:@"image not found"]);
}
#endif

- (void)testCrashWithAnnotations {
  [_rootObject crashWithAnnotations];
  [self verifyCrashReportSignal:SIGABRT];
  NSDictionary* dict = [_rootObject getAnnotations];

  NSDictionary* simpleMap = dict[@"simplemap"];
  XCTAssertTrue([simpleMap[@"#TEST# empty_value"] isEqualToString:@""]);
  XCTAssertTrue([simpleMap[@"#TEST# key"] isEqualToString:@"value"]);
  XCTAssertTrue([simpleMap[@"#TEST# longer"] isEqualToString:@"shorter"]);
  XCTAssertTrue([simpleMap[@"#TEST# pad"] isEqualToString:@"crash"]);
  XCTAssertTrue([simpleMap[@"#TEST# x"] isEqualToString:@"y"]);

  XCTAssertTrue([[dict[@"objects"][0] valueForKeyPath:@"#TEST# same-name"]
      isEqualToString:@"same-name 4"]);
  XCTAssertTrue([[dict[@"objects"][1] valueForKeyPath:@"#TEST# same-name"]
      isEqualToString:@"same-name 3"]);
  XCTAssertTrue([[dict[@"objects"][2] valueForKeyPath:@"#TEST# one"]
      isEqualToString:@"moocow"]);
}

@end
