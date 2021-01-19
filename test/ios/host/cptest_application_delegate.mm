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

#import "test/ios/host/cptest_application_delegate.h"

#include <dispatch/dispatch.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <vector>

#import "Service/Sources/EDOHostNamingService.h"
#import "Service/Sources/EDOHostService.h"
#include "client/crash_report_database.h"
#include "client/crashpad_client.h"
#include "snapshot/minidump/process_snapshot_minidump.h"
#import "test/ios/host/cptest_crash_view_controller.h"
#import "test/ios/host/cptest_shared_object.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CPTestApplicationDelegate {
  crashpad::CrashpadClient client_;
  std::unique_ptr<crashpad::CrashReportDatabase> database_;
}

@synthesize window = _window;

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  // Start up crashpad.
  base::FilePath database_dir([NSFileManager.defaultManager
                                  URLsForDirectory:NSDocumentDirectory
                                         inDomains:NSUserDomainMask]
                                  .lastObject.path.UTF8String);
  database_dir = database_dir.Append("crashpad");
  database_ = crashpad::CrashReportDatabase::Initialize(database_dir);
  std::vector<crashpad::CrashReportDatabase::Report> all_reports;
  crashpad::CrashReportDatabase::OperationStatus status;
  status = database_->GetPendingReports(&all_reports);
  std::vector<crashpad::CrashReportDatabase::Report> completed_reports;
  status = database_->GetCompletedReports(&completed_reports);
  all_reports.insert(
      all_reports.end(), completed_reports.begin(), completed_reports.end());
  for (auto report : all_reports) {
    database_->DeleteReport(report.uuid);
  }

  base::FilePath metrics_dir;
  client_.StartCrashpadInProcessHandler(
      database_dir,
      metrics_dir,
      "https://clients2.google.com/cr/staging_report",
      {{"prod", "ios_crash_xcuitests"}, {"ver", "1"}, {"plat", "iPhoneos"}});
  self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
  [self.window makeKeyAndVisible];
  self.window.backgroundColor = UIColor.greenColor;

  CPTestCrashViewController* controller =
      [[CPTestCrashViewController alloc] init];
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

- (bool)verifyLastException:(int)type {
  std::vector<crashpad::CrashReportDatabase::Report> all_reports;
  crashpad::CrashReportDatabase::OperationStatus status;
  base::FilePath database_dir([NSFileManager.defaultManager
                                  URLsForDirectory:NSDocumentDirectory
                                         inDomains:NSUserDomainMask]
                                  .lastObject.path.UTF8String);
  database_dir = database_dir.Append("crashpad");
  std::unique_ptr<crashpad::CrashReportDatabase> database =
      crashpad::CrashReportDatabase::Initialize(database_dir);
  status = database->GetPendingReports(&all_reports);
  if (status != crashpad::CrashReportDatabase::kNoError) {
    return false;
  }
  std::vector<crashpad::CrashReportDatabase::Report> completed_reports;
  status = database->GetCompletedReports(&completed_reports);
  all_reports.insert(
      all_reports.end(), completed_reports.begin(), completed_reports.end());
  if (status != crashpad::CrashReportDatabase::kNoError) {
    return false;
  }
  if (all_reports.size() != 1) {
    return false;
  }

  auto reader = std::make_unique<crashpad::FileReader>();
  reader->Open(all_reports[0].file_path);
  crashpad::ProcessSnapshotMinidump process_snapshot;
  process_snapshot.Initialize(reader.get());
  return static_cast<int>(process_snapshot.Exception()->Exception()) == type;
}

- (void)crashBadAccess {
  strcpy(nullptr, "bla");
}

- (void)crashKillAbort {
  kill(getpid(), SIGABRT);
}

- (void)crashSegv {
  long* zero = nullptr;
  *zero = 0xc045004d;
}

- (void)crashTrap {
  __builtin_trap();
}

- (void)crashAbort {
  abort();
}

- (void)crashException {
  std::vector<int> empty_vector = {};
  empty_vector.at(42);
}

- (void)crashNSException {
  // EDO has its own sinkhole.
  dispatch_async(dispatch_get_main_queue(), ^{
    NSArray* empty_array = @[];
    [empty_array objectAtIndex:42];
  });
}

- (void)catchNSException {
  @try {
    NSArray* empty_array = @[];
    [empty_array objectAtIndex:42];
  } @catch (NSException* exception) {
  } @finally {
  }
}

- (void)crashUnreocgnizedSelectorAfterDelay {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundeclared-selector"
  [self performSelector:@selector(does_not_exist) withObject:nil afterDelay:1];
#pragma clang diagnostic pop
}

- (void)recurse {
  [self recurse];
}

- (void)crashRecursion {
  [self recurse];
}

@end
