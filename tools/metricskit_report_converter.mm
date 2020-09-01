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

#import <Foundation/Foundation.h>

#include <string>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "minidump/minidump_file_writer.h"
#include "snapshot/metricskit/process_snapshot_metricskit.h"
#include "tools/tool_support.h"
#include "util/file/file_writer.h"

namespace crashpad {
namespace {

int MetricsKitReportConverterMain(int argc, char** argv) {
  if (argc < 3) {
    LOG(FATAL) << "Usage: metricskit_report_converter <input> <output>";
  }

  std::string input(argv[1]);
  std::string output(argv[2]);

  LOG(ERROR) << input;
  LOG(ERROR) << output;

  NSData* data = [NSData dataWithContentsOfFile:base::SysUTF8ToNSString(input)];
  id json = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
  NSDictionary* dict = base::mac::ObjCCastStrict<NSDictionary>(json);
  NSArray* reports =
      base::mac::ObjCCastStrict<NSArray>(dict[@"crashDiagnostics"]);

  for (id item in reports) {
    NSDictionary* report = base::mac::ObjCCastStrict<NSDictionary>(item);

    NSLog(@"Processing report %p...", report);
    ProcessSnapshotMetricsKit process_snapshot;
    process_snapshot.Initialize(report);

    FileWriter file_writer;
    base::FilePath dump_path(
        ToolSupport::CommandLineArgumentToFilePathStringType(output));
    if (!file_writer.Open(dump_path,
                          FileWriteMode::kTruncateOrCreate,
                          FilePermissions::kWorldReadable)) {
      return EXIT_FAILURE;
    }

    MinidumpFileWriter minidump;
    minidump.InitializeFromSnapshot(&process_snapshot);
    if (!minidump.WriteEverything(&file_writer)) {
      file_writer.Close();
      if (unlink(output.c_str()) != 0) {
        PLOG(ERROR) << "unlink";
      }
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}

}  // namespace
}  // namespace crashpad

int main(int argc, char** argv) {
  crashpad::MetricsKitReportConverterMain(argc, argv);
}
