// Copyright 2019 The Crashpad Authors. All rights reserved.
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

#include <stdio.h>
#include <unistd.h>
#include <string>

#include "base/files/file_path.h"
#include "util/stream/log_file_processor.h"

namespace {

struct Options {
  bool encoding;
  std::string input_file;
  std::string output_file;
};

static void Usage(int argc, const char* argv[], bool error) {
  fprintf(error ? stderr : stdout,
          "Usage: %s [options] <input-file> <output-file>\n"
          "\n"
          "Encode/Decode minidump\n"
          "\n"
          "Options:\n"
          "\n"
          "  -e         Compress and encode the input file to a base94\n"
          "             encoded file\n"
          "  -d         Decode and decompress a base94 encoded file\n",
          base::FilePath(argv[0]).BaseName().value().c_str());
}

void SetupOptions(int argc, const char* argv[], Options* options) {
  int ch;
  bool encoding_valid = false;
  while ((ch = getopt(argc, (char* const*)argv, "hed")) != -1) {
    switch (ch) {
      case 'h':
        Usage(argc, argv, false);
        exit(0);
        break;
      case '?':
        Usage(argc, argv, true);
        exit(1);
        break;
      case 'e':
        encoding_valid = true;
        options->encoding = true;
        break;
      case 'd':
        encoding_valid = true;
        options->encoding = false;
        break;
    }
  }
  if (!encoding_valid) {
    Usage(argc, argv, true);
    exit(1);
  }
  if (argc - optind != 2) {
    Usage(argc, argv, true);
    exit(1);
  }

  options->input_file = argv[optind];
  options->output_file = argv[optind + 1];
}

}  // namespace

int main(int argc, const char* argv[]) {
  Options options{};
  SetupOptions(argc, argv, &options);
  crashpad::LogFileProcessor processor(
      options.encoding ? crashpad::LogFileProcessor::Mode::kEncode
                       : crashpad::LogFileProcessor::Mode::kDecode,
      base::FilePath(options.input_file),
      base::FilePath(options.output_file));
  return processor.Process() ? 0 : 1;
}
