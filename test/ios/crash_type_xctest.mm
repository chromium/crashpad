// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>
#import "test/ios/host/edo_placeholder.h"
#import "third_party/edo/src/Service/Sources/EDOClientService.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CrashTypeTestCase : XCTestCase
@end

@implementation CrashTypeTestCase

- (void)setUp {
  [[[XCUIApplication alloc] init] launch];
}

- (void)testEDO {
  EDOPlaceholder* rootObject = [EDOClientService rootObjectWithPort:12345];
  NSString* result = [rootObject testEDO];
  XCTAssertTrue([result isEqualToString:@"crashpad"],
                @"Strings are not equal %@ %@",
                result,
                @"crashpad");
}

@end
