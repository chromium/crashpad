// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CrashTypeTestCase : XCTestCase
@end

@implementation CrashTypeTestCase

- (void)setUp {
  [[[XCUIApplication alloc] init] launch];
}

- (void)testSuccess {
  XCTAssertTrue(YES);
}

- (void)testFail {
  XCTAssertTrue(NO);
}

@end
