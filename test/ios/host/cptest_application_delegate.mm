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
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <mach-o/dyld_images.h>
#include <mach-o/nlist.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <vector>

#import "Service/Sources/EDOHostNamingService.h"
#import "Service/Sources/EDOHostService.h"
#import "Service/Sources/NSObject+EDOValueObject.h"
#include "client/annotation.h"
#include "client/annotation_list.h"
#include "client/crash_report_database.h"
#include "client/crashpad_client.h"
#include "client/crashpad_info.h"
#include "client/simple_string_dictionary.h"
#include "snapshot/minidump/process_snapshot_minidump.h"
#import "test/ios/host/cptest_crash_view_controller.h"
#import "test/ios/host/cptest_shared_object.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

base::FilePath GetDatabaseDir() {
  base::FilePath database_dir([NSFileManager.defaultManager
                                  URLsForDirectory:NSDocumentDirectory
                                         inDomains:NSUserDomainMask]
                                  .lastObject.path.UTF8String);
  return database_dir.Append("crashpad");
}

std::unique_ptr<crashpad::CrashReportDatabase> GetDatabase() {
  base::FilePath database_dir = GetDatabaseDir();
  std::unique_ptr<crashpad::CrashReportDatabase> database =
      crashpad::CrashReportDatabase::Initialize(database_dir);
  return database;
}

[[clang::optnone]] void recurse(int counter) {
  // Fill up the stack faster
  int arr[1024];
  arr[0] = counter;
  if (counter > INT_MAX)
    return;
  recurse(++counter);
}
}

@implementation CPTestApplicationDelegate

@synthesize window = _window;

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  // Start up crashpad.
  base::FilePath metrics_dir;
  crashpad::CrashpadClient client;
  client.StartCrashpadInProcessHandler(
      GetDatabaseDir(),
      metrics_dir,
      // Uncomment to break tests but test uploading.
      "https://clients2.google.com/cr/staging_report",
      {{"prod", "ios_crash_xcuitests"}, {"ver", "2"}, {"plat", "iPhoneos"}});
      // "",
      //  {});

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

- (void)clearReports {
  crashpad::CrashReportDatabase::OperationStatus status;
  std::unique_ptr<crashpad::CrashReportDatabase> database(GetDatabase());
  std::vector<crashpad::CrashReportDatabase::Report> pending_reports;
  status = database->GetPendingReports(&pending_reports);
  for (auto report : pending_reports) {
    database->DeleteReport(report.uuid);
  }
}
- (int)pendingReportCount {
  crashpad::CrashReportDatabase::OperationStatus status;
  std::unique_ptr<crashpad::CrashReportDatabase> database(GetDatabase());
  std::vector<crashpad::CrashReportDatabase::Report> pending_reports;
  status = database->GetPendingReports(&pending_reports);
  if (status != crashpad::CrashReportDatabase::kNoError) {
    return -1;
  }

  return pending_reports.size();
}

- (int)lastException {
  crashpad::CrashReportDatabase::OperationStatus status;
  std::unique_ptr<crashpad::CrashReportDatabase> database(GetDatabase());
  std::vector<crashpad::CrashReportDatabase::Report> pending_reports;
  status = database->GetPendingReports(&pending_reports);
  if (status != crashpad::CrashReportDatabase::kNoError) {
    return -1;
  }
  if (pending_reports.size() != 1) {
    return -2;
  }

  auto reader = std::make_unique<crashpad::FileReader>();
  reader->Open(pending_reports[0].file_path);
  crashpad::ProcessSnapshotMinidump process_snapshot;
  process_snapshot.Initialize(reader.get());
  return static_cast<int>(process_snapshot.Exception()->Exception());
}

- (NSString*)crashInfoMessage:(bool)first {
  crashpad::CrashReportDatabase::OperationStatus status;
  std::unique_ptr<crashpad::CrashReportDatabase> database(GetDatabase());
  std::vector<crashpad::CrashReportDatabase::Report> pending_reports;
  status = database->GetPendingReports(&pending_reports);
  if (status != crashpad::CrashReportDatabase::kNoError) {
    return @"";
  }
  if (pending_reports.size() != 1) {
    return @"";
  }

  auto reader = std::make_unique<crashpad::FileReader>();
  reader->Open(pending_reports[0].file_path);
  crashpad::ProcessSnapshotMinidump process_snapshot;
  process_snapshot.Initialize(reader.get());

  std::vector<std::string> strings;
  for (auto& module : process_snapshot.Modules()) {
    for (auto& string : module->AnnotationsVector()) {
      strings.push_back(string);
    }
  }

  if (first)
    return [[NSString alloc] initWithUTF8String:strings.front().c_str()];
  else
    return [[NSString alloc] initWithUTF8String:strings.back().c_str()];
}

- (NSDictionary*)getAnnotations {
  crashpad::CrashReportDatabase::OperationStatus status;
  std::unique_ptr<crashpad::CrashReportDatabase> database(GetDatabase());
  std::vector<crashpad::CrashReportDatabase::Report> pending_reports;
  status = database->GetPendingReports(&pending_reports);
  if (status != crashpad::CrashReportDatabase::kNoError) {
    return @{};
  }
  if (pending_reports.size() != 1) {
    return @{};
  }

  auto reader = std::make_unique<crashpad::FileReader>();
  reader->Open(pending_reports[0].file_path);
  crashpad::ProcessSnapshotMinidump process_snapshot;
  process_snapshot.Initialize(reader.get());

  std::string process_name =
      [[[NSProcessInfo processInfo] processName] UTF8String];
  for (const auto* module : process_snapshot.Modules()) {
    if (module->Name().find(process_name) == std::string::npos)
      continue;
    NSDictionary* dict = @{
      @"simplemap" : [@{} mutableCopy],
      @"vector" : [@[] mutableCopy],
      @"objects" : [@[] mutableCopy]
    };
    for (const auto& kv : module->AnnotationsSimpleMap()) {
      [dict[@"simplemap"] setValue:@(kv.second.c_str())
                            forKey:@(kv.first.c_str())];
    }

    for (const std::string& annotation : module->AnnotationsVector()) {
      [dict[@"vector"] addObject:@(annotation.c_str())];
    }

    for (const auto& annotation : module->AnnotationObjects()) {
      if (annotation.type !=
          static_cast<uint16_t>(crashpad::Annotation::Type::kString)) {
        continue;
      }
      std::string value(reinterpret_cast<const char*>(annotation.value.data()),
                        annotation.value.size());
      [dict[@"objects"]
          addObject:@{@(annotation.name.c_str()) : @(value.c_str())}];
    }
    return [dict passByValue];
  }
  return [@{} passByValue];
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

- (void)crashRecursion {
  recurse(0);
}

- (void)crashWithCrashInfoMessage {
  dlsym(nullptr, nullptr);
}

- (void)crashWithDyldErrorString {
  task_dyld_info_data_t dyld_info;
  mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
  kern_return_t kr = task_info(mach_task_self(),
                               TASK_DYLD_INFO,
                               reinterpret_cast<task_info_t>(&dyld_info),
                               &count);
  if (kr != KERN_SUCCESS) {
    return;
  }

  const dyld_all_image_infos* image_infos =
      reinterpret_cast<dyld_all_image_infos*>(dyld_info.all_image_info_addr);

  uint64_t address =
      crashpad::FromPointerCast<uint64_t>(image_infos->dyldImageLoadAddress);

  const mach_header_64* header = (struct mach_header_64*)address;
  vm_size_t slide = 0;
  const load_command* command =
      reinterpret_cast<const load_command*>(header + 1);
  const symtab_command* symtab_command = nullptr;
  const dysymtab_command* dysymtab_command = nullptr;
  const segment_command_64* linkedit_seg = nullptr;
  const segment_command_64* text_seg = nullptr;
  for (uint32_t cmd_index = 0, cumulative_cmd_size = 0;
       cmd_index <= header->ncmds && cumulative_cmd_size < header->sizeofcmds;
       ++cmd_index, cumulative_cmd_size += command->cmdsize) {
    if (command->cmd == LC_SEGMENT_64) {
      const segment_command_64* segment =
          reinterpret_cast<const segment_command_64*>(command);
      if (strcmp(segment->segname, SEG_TEXT) == 0) {
        text_seg = segment;
        slide = address - segment->vmaddr;
      } else if (strcmp(segment->segname, SEG_LINKEDIT) == 0) {
        linkedit_seg = segment;
      }
    } else if (command->cmd == LC_SYMTAB) {
      symtab_command = reinterpret_cast<const struct symtab_command*>(command);
    } else if (command->cmd == LC_DYSYMTAB) {
      dysymtab_command =
          reinterpret_cast<const struct dysymtab_command*>(command);
    }

    command = reinterpret_cast<const load_command*>(
        reinterpret_cast<const uint8_t*>(command) + command->cmdsize);
  }

  if (text_seg == nullptr || linkedit_seg == nullptr ||
      symtab_command == nullptr)
    return;

  uint64_t file_slide =
      (linkedit_seg->vmaddr - text_seg->vmaddr) - linkedit_seg->fileoff;
  uint64_t strings = address + (symtab_command->stroff + file_slide);
  nlist_64* symbol = reinterpret_cast<nlist_64*>(
      address + (symtab_command->symoff + file_slide));

  for (uint32_t i = 0; i < symtab_command->nsyms; i++, symbol++) {
    if (!symbol->n_value)
      continue;

    if (strcmp(reinterpret_cast<const char*>(strings + symbol->n_un.n_strx),
               "_NSAddImage") == 0) {
      const struct mach_header* (*NSAddImage)(const char* path,
                                              uint32_t options);
      NSAddImage =
          (const struct mach_header* (*)(const char* path, uint32_t options))(
              symbol->n_value + slide);
#define NSADDIMAGE_OPTION_RETURN_ON_ERROR 0x1
      NSAddImage("/path/to/nowhere", NSADDIMAGE_OPTION_RETURN_ON_ERROR);
      break;
    }
    continue;
  }
  abort();
}

- (void)crashWithAnnotations {
  // This is “leaked” to crashpad_info.
  crashpad::SimpleStringDictionary* simple_annotations =
      new crashpad::SimpleStringDictionary();
  simple_annotations->SetKeyValue("#TEST# pad", "break");
  simple_annotations->SetKeyValue("#TEST# key", "value");
  simple_annotations->SetKeyValue("#TEST# pad", "crash");
  simple_annotations->SetKeyValue("#TEST# x", "y");
  simple_annotations->SetKeyValue("#TEST# longer", "shorter");
  simple_annotations->SetKeyValue("#TEST# empty_value", "");

  crashpad::CrashpadInfo* crashpad_info =
      crashpad::CrashpadInfo::GetCrashpadInfo();

  crashpad_info->set_simple_annotations(simple_annotations);

  crashpad::AnnotationList::Register();  // This is “leaked” to crashpad_info.

  static crashpad::StringAnnotation<32> test_annotation_one{"#TEST# one"};
  static crashpad::StringAnnotation<32> test_annotation_two{"#TEST# two"};
  static crashpad::StringAnnotation<32> test_annotation_three{
      "#TEST# same-name"};
  static crashpad::StringAnnotation<32> test_annotation_four{
      "#TEST# same-name"};

  test_annotation_one.Set("moocow");
  test_annotation_two.Set("this will be cleared");
  test_annotation_three.Set("same-name 3");
  test_annotation_four.Set("same-name 4");
  test_annotation_two.Clear();
  abort();
}

@end
