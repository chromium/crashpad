// Copyright 2018 The Crashpad Authors. All rights reserved.
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

#include "snapshot/sanitized/process_snapshot_sanitized.h"

#include "base/macros.h"
#include "build/build_config.h"
#include "gtest/gtest.h"
#include "test/multiprocess_exec.h"
#include "util/file/file_io.h"
#include "util/numeric/safe_assignment.h"

#if defined(OS_LINUX) || defined(OS_ANDROID)
#include <sys/syscall.h>

#include "snapshot/linux/process_snapshot_linux.h"
#include "util/linux/direct_ptrace_connection.h"
#include "util/linux/exception_information.h"
#include "util/posix/signals.h"
#endif

namespace crashpad {
namespace test {
namespace {

class ExceptionGenerator {
 public:
  static ExceptionGenerator* Get() {
    static ExceptionGenerator* instance = new ExceptionGenerator();
    return instance;
  }

  bool Initialize(FileHandle in, FileHandle out) {
    in_ = in;
    out_ = out;
    return Signals::InstallCrashHandlers(HandleCrash, 0, nullptr);
  }

 private:
  ExceptionGenerator() = default;
  ~ExceptionGenerator() = delete;

  static void HandleCrash(int signo, siginfo_t* siginfo, void* context) {
    auto state = Get();

    ExceptionInformation info = {};
    info.siginfo_address = FromPointerCast<VMAddress>(siginfo);
    info.context_address = FromPointerCast<VMAddress>(context);
    info.thread_id = syscall(SYS_gettid);

    auto info_addr = FromPointerCast<VMAddress>(&info);
    ASSERT_TRUE(LoggingWriteFile(state->out_, &info_addr, sizeof(info_addr)));

    CheckedReadFileAtEOF(state->in_);
    Signals::RestoreHandlerAndReraiseSignalOnReturn(siginfo, nullptr);
  }

  FileHandle in_;
  FileHandle out_;

  DISALLOW_COPY_AND_ASSIGN(ExceptionGenerator);
};

constexpr char kWhitelistedAnnotationName[] = "name_of_whitelisted_anno";
constexpr char kWhitelistedAnnotationValue[] = "some_value";
constexpr char kNonWhitelistedAnnotationName[] = "non_whitelisted_anno";
constexpr char kNonWhitelistedAnnotationValue[] = "private_annotation";
constexpr char kSensitiveStackData[] = "sensitive_stack_data";

void ChildTestFunction() {
  FileHandle in = StdioFileHandle(StdioStream::kStandardInput);
  FileHandle out = StdioFileHandle(StdioStream::kStandardOutput);

  static StringAnnotation<32> whitelisted_annotation(
      kWhitelistedAnnotationName);
  whitelisted_annotation.Set(kWhitelistedAnnotationValue);

  static StringAnnotation<32> non_whitelisted_annotation(
      kNonWhitelistedAnnotationName);
  non_whitelisted_annotation.Set(kNonWhitelistedAnnotationValue);

  char stack_data[strlen(kSensitiveStackData) + 1];
  strcpy(stack_data, kSensitiveStackData);

  auto stack_data_addr = FromPointerCast<VMAddress>(stack_data);
  ASSERT_TRUE(LoggingWriteFile(out, &stack_data_addr, sizeof(stack_data_addr)));

  auto module_address = FromPointerCast<VMAddress>(ChildTestFunction);
  ASSERT_TRUE(LoggingWriteFile(out, &module_address, sizeof(module_address)));

  auto non_module_address = FromPointerCast<VMAddress>(&module_address);
  ASSERT_TRUE(
      LoggingWriteFile(out, &non_module_address, sizeof(non_module_address)));

  auto gen = ExceptionGenerator::Get();
  ASSERT_TRUE(gen->Initialize(in, out));

  __builtin_trap();
}

CRASHPAD_CHILD_TEST_MAIN(ChildToBeSanitized) {
  ChildTestFunction();
  NOTREACHED();
  return EXIT_SUCCESS;
}

void ExpectAnnotations(ProcessSnapshot* snapshot, bool sanitized) {
  bool found_whitelisted = false;
  bool found_non_whitelisted = false;
  for (auto module : snapshot->Modules()) {
    for (const auto& anno : module->AnnotationObjects()) {
      if (anno.name == kWhitelistedAnnotationName) {
        found_whitelisted = true;
      } else if (anno.name == kNonWhitelistedAnnotationName) {
        found_non_whitelisted = true;
      }
    }
  }

  EXPECT_TRUE(found_whitelisted);
  if (sanitized) {
    EXPECT_FALSE(found_non_whitelisted);
  } else {
    EXPECT_TRUE(found_non_whitelisted);
  }
}

class ReadStringFromStack : public MemorySnapshot::Delegate {
 public:
  ReadStringFromStack() = default;
  ~ReadStringFromStack() = default;

  std::string operator()(const MemorySnapshot* stack, VMAddress string_addr) {
    stack_string_.clear();
    stack_ = stack;
    string_addr_ = string_addr;
    EXPECT_TRUE(stack_->Read(this));
    return stack_string_;
  }

  // MemorySnapshot::Delegate
  bool MemorySnapshotDelegateRead(void* data, size_t size) override {
    size_t offset;
    if (!AssignIfInRange(&offset, string_addr_ - stack_->Address())) {
      return false;
    }

    auto string = static_cast<char*>(data) + offset;
    size_t length = strnlen(string, size - offset);
    if (length == size - offset) {
      return false;
    }

    stack_string_.insert(0, string, length);
    return true;
  }

 private:
  std::string stack_string_;
  const MemorySnapshot* stack_;
  VMAddress string_addr_;
};

void ExpectStackData(ProcessSnapshot* snapshot,
                     VMAddress data_addr,
                     bool sanitized) {
  const ThreadSnapshot* crasher = nullptr;
  for (const auto thread : snapshot->Threads()) {
    if (thread->ThreadID() == snapshot->Exception()->ThreadID()) {
      crasher = thread;
      break;
    }
  }
  ASSERT_TRUE(crasher);

  const MemorySnapshot* stack = crasher->Stack();
  std::string stack_string = ReadStringFromStack()(stack, data_addr);

  if (sanitized) {
    EXPECT_NE(stack_string, kSensitiveStackData);
  } else {
    EXPECT_EQ(stack_string, kSensitiveStackData);
  }
}

class SanitizeTest : public MultiprocessExec {
 public:
  SanitizeTest() : MultiprocessExec() {
    SetChildTestMainFunction("ChildToBeSanitized");
    SetExpectedChildTerminationBuiltinTrap();
  }

  ~SanitizeTest() = default;

 private:
  void MultiprocessParent() {
    VMAddress stack_data_addr;
    ASSERT_TRUE(LoggingReadFileExactly(
        ReadPipeHandle(), &stack_data_addr, sizeof(stack_data_addr)));

    VMAddress module_address;
    ASSERT_TRUE(LoggingReadFileExactly(
        ReadPipeHandle(), &module_address, sizeof(module_address)));

    VMAddress non_module_address;
    ASSERT_TRUE(LoggingReadFileExactly(
        ReadPipeHandle(), &non_module_address, sizeof(non_module_address)));

    VMAddress exception_info_addr;
    ASSERT_TRUE(LoggingReadFileExactly(
        ReadPipeHandle(), &exception_info_addr, sizeof(exception_info_addr)));

    DirectPtraceConnection connection;
    ASSERT_TRUE(connection.Initialize(ChildProcess()));

    // MemoryMap map;
    // ASSERT_TRUE(map.Initialize(&connection));
    // map.Print();

    ProcessSnapshotLinux snapshot;
    ASSERT_TRUE(snapshot.Initialize(&connection));
    ASSERT_TRUE(snapshot.InitializeException(exception_info_addr));

    ExpectAnnotations(&snapshot, /* sanitized= */ false);
    ExpectStackData(&snapshot, stack_data_addr, /* sanitized= */ false);

    std::vector<std::string> whitelist;
    whitelist.push_back(kWhitelistedAnnotationName);

    ProcessSnapshotSanitized sanitized;
    ASSERT_TRUE(
        sanitized.Initialize(&snapshot, &whitelist, module_address, false));

    ExpectAnnotations(&sanitized, /* sanitized= */ true);
    ExpectStackData(&sanitized, stack_data_addr, /* sanitized= */ true);

    ProcessSnapshotSanitized screened_snapshot;
    EXPECT_FALSE(screened_snapshot.Initialize(
        &snapshot, nullptr, non_module_address, false));
  }

  DISALLOW_COPY_AND_ASSIGN(SanitizeTest);
};

TEST(ProcessSnapshotSanitized, Sanitize) {
  SanitizeTest test;
  test.Run();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
