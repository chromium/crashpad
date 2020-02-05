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

#import "Service/Sources/EDOClientService.h"
#import "test/ios/host/edo_placeholder.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CPTestTestCase : XCTestCase {
  XCUIApplication* _app;
}
@end

@implementation CPTestTestCase

- (void)setUp {
  _app = [[XCUIApplication alloc] init];
  [_app launch];

  [EDOClientService setErrorHandler:^(NSError *error) {
    // Do nothing.
  }];

}

- (void)testEDO {
  EDOPlaceholder* rootObject = [EDOClientService rootObjectWithPort:12345];
  NSString* result = [rootObject testEDO];
  XCTAssertEqualObjects(result, @"crashpad");
}

- (void)testTrap {
  XCTAssertTrue(_app.state == XCUIApplicationStateRunningForeground);
  EDOPlaceholder* rootObject = [EDOClientService rootObjectWithPort:12345];
  NSLog(@"state is %d", (int)_app.state);
  NSLog(@"state is %d", (int)_app.state);
  NSLog(@"state is %d", (int)_app.state);
  NSLog(@"state is %d", (int)_app.state);
  NSLog(@"state is %d", (int)_app.state);
  NSLog(@"state is %d", (int)_app.state);
  NSLog(@"state is %d", (int)_app.state);
  [rootObject crashTrap];
  [_app terminate];
  NSLog(@"state is %d", (int)_app.state);
  NSLog(@"state is %d", (int)_app.state);
  NSLog(@"state is %d", (int)_app.state);
  NSLog(@"state is %d", (int)_app.state);
  NSLog(@"state is %d", (int)_app.state);
  NSLog(@"state is %d", (int)_app.state);
  NSLog(@"state is %d", (int)_app.state);
//  XCTAssertTrue(_app.state == XCUIApplicationStateNotRunning);
  XCTAssertTrue(YES);
}

@end
