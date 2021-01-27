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

#include "util/ios/exception_processor.h"

#import <Foundation/Foundation.h>
#include <TargetConditionals.h>
#include <cxxabi.h>
#include <dlfcn.h>
#include <libunwind.h>
#include <mach-o/loader.h>
#include <objc/message.h>
#include <objc/objc-exception.h>
#include <objc/objc.h>
#include <objc/runtime.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <unwind.h>

#include <exception>
#include <type_traits>
#include <typeinfo>

#include "base/bit_cast.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/memory/free_deleter.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "build/build_config.h"
#include "client/annotation.h"

namespace {

// From 10.15.0 objc4-779.1/runtime/objc-exception.mm.
struct objc_typeinfo {
  const void* const* vtable;
  const char* name;
  Class cls_unremapped;
};
struct objc_exception {
  id obj;
  objc_typeinfo tinfo;
};

// From 10.15.0 objc4-779.1/runtime/objc-abi.h.
extern "C" const void* const objc_ehtype_vtable[];

// https://github.com/llvm/llvm-project/blob/09dc884eb2e4/libcxxabi/src/cxa_exception.h
static const uint64_t kOurExceptionClass = 0x434c4e47432b2b00;
struct __cxa_exception {
#if defined(ARCH_CPU_64_BITS)
  void* reserve;
  size_t referenceCount;
#endif
  std::type_info* exceptionType;
  void (*exceptionDestructor)(void*);
  std::unexpected_handler unexpectedHandler;
  std::terminate_handler terminateHandler;
  __cxa_exception* nextException;
  int handlerCount;
  int handlerSwitchValue;
  const unsigned char* actionRecord;
  const unsigned char* languageSpecificData;
  void* catchTemp;
  void* adjustedPtr;
#if !defined(ARCH_CPU_64_BITS)
  size_t referenceCount;
#endif
  _Unwind_Exception unwindHeader;
};

std::string FormatStackTrace(NSArray<NSNumber*>* addresses, size_t max_length) {
  std::string value;
  for (NSNumber* address in addresses) {
    uint64_t addr = [address unsignedLongLongValue];
    std::string str = base::StringPrintf("0x%" PRIx64, addr);
    if (value.size() + str.size() > max_length)
      break;
    value += str + " ";
  }

  if (!value.empty() && value.back() == ' ') {
    value.resize(value.size() - 1);
  }

  return value;
}

objc_exception_preprocessor g_next_preprocessor;
bool g_exception_preprocessor_installed;

void TerminatingFromUncaughtNSException(id exception, const char* sinkhole) {
  std::string name = base::SysNSStringToUTF8([exception name]);
  std::string reason = base::SysNSStringToUTF8([exception reason]);
  static crashpad::StringAnnotation<256> nameKey("exceptionName");
  nameKey.Set(name);
  static crashpad::StringAnnotation<512> reasonKey("exceptionReason");
  reasonKey.Set(reason);

  LOG(FATAL) << "Terminating from Objective-C exception name: " << name
             << " reason: " << reason << " with sinkhole: " << sinkhole;
}

// Returns true if |path| equals |sinkhole| on device. Simulator paths prepend
// much of Xcode's internal structure, so check that |path| ends with |sinkhole|
// for simulator.
bool ModulePathMatchesSinkhole(const char* path, const char* sinkhole) {
#if TARGET_OS_SIMULATOR
  size_t path_length = strlen(path);
  size_t sinkhole_length = strlen(sinkhole);
  if (sinkhole_length > path_length)
    return false;
  return strncmp(path + path_length - sinkhole_length,
                 sinkhole,
                 sinkhole_length) == 0;
#else
  return strcmp(path, sinkhole) == 0;
#endif
}

int LoggingUnwStep(unw_cursor_t* cursor) {
  int rv = unw_step(cursor);
  if (rv < 0) {
    LOG(ERROR) << "unw_step: " << rv;
  }
  return rv;
}

id ObjcExceptionPreprocessor(id exception) {
  static bool seen_first_exception = false;
  // Record UMA and crash keys about the exception.
  //  RecordExceptionWithUma(exception);
  static crashpad::StringAnnotation<256> firstexception("firstexception");
  static crashpad::StringAnnotation<256> lastexception("lastexception");
  static crashpad::StringAnnotation<1024> firstexception_bt(
      "firstexception_bt");
  static crashpad::StringAnnotation<1024> lastexception_bt("lastexception_bt");
  auto* key = seen_first_exception ? &lastexception : &firstexception;
  auto* bt_key = seen_first_exception ? &lastexception_bt : &firstexception_bt;
  NSString* value = [NSString
      stringWithFormat:@"%@ reason %@", [exception name], [exception reason]];
  key->Set(base::SysNSStringToUTF8(value));
  // This exception preprocessor runs prior to the one in libobjc, which sets
  // the -[NSException callStackReturnAddresses].
  std::string trace_string =
      FormatStackTrace([NSThread callStackReturnAddresses], 1024);
  bt_key->Set(trace_string);
  seen_first_exception = true;

  // Unwind the stack looking for any exception handlers. If an exception
  // handler is encountered, test to see if it is a function known to catch-
  // and-rethrow as a "top-level" exception handler. Various routines in
  // Cocoa/UIKit do this, and it obscures the crashing stack, since the original
  // throw location is no longer present on the stack (just the re-throw) when
  // Crashpad captures the crash report.
  unw_context_t context;
  unw_getcontext(&context);

  unw_cursor_t cursor;
  unw_init_local(&cursor, &context);

  static const void* this_base_address = []() -> const void* {
    Dl_info dl_info;
    if (!dladdr(reinterpret_cast<const void*>(&ObjcExceptionPreprocessor),
                &dl_info)) {
      LOG(ERROR) << "dladdr: " << dlerror();
      return nullptr;
    }
    return dl_info.dli_fbase;
  }();

  // Generate an exception_header for the __personality_routine.
  // From 10.15.0 objc4-779.1/runtime/objc-exception.mm objc_exception_throw.
  objc_exception* exception_objc = reinterpret_cast<objc_exception*>(
      __cxxabiv1::__cxa_allocate_exception(sizeof(objc_exception)));
  exception_objc->obj = exception;
  exception_objc->tinfo.vtable = objc_ehtype_vtable + 2;
  exception_objc->tinfo.name = object_getClassName(exception);
  exception_objc->tinfo.cls_unremapped = object_getClass(exception);

  // https://github.com/llvm/llvm-project/blob/c5d2746fbea7/libcxxabi/src/cxa_exception.cpp
  // __cxa_throw
  __cxa_exception* exception_header =
      reinterpret_cast<__cxa_exception*>(exception_objc) - 1;
  exception_header->unexpectedHandler = std::get_unexpected();
  exception_header->terminateHandler = std::get_terminate();
  exception_header->exceptionType =
      reinterpret_cast<std::type_info*>(&exception_objc->tinfo);
  exception_header->unwindHeader.exception_class = kOurExceptionClass;

  bool handler_found = false;
  while (LoggingUnwStep(&cursor) > 0) {
    unw_proc_info_t frame_info;
    if (unw_get_proc_info(&cursor, &frame_info) != UNW_ESUCCESS) {
      continue;
    }

    if (frame_info.handler == 0) {
      continue;
    }

    // Check to see if the handler is really an exception handler.
    __personality_routine p =
        reinterpret_cast<__personality_routine>(frame_info.handler);

    // From 10.15.0 libunwind-35.4/src/UnwindLevel1.c.
    _Unwind_Reason_Code personalityResult = (*p)(
        1,
        _UA_SEARCH_PHASE,
        exception_header->unwindHeader.exception_class,
        reinterpret_cast<_Unwind_Exception*>(&exception_header->unwindHeader),
        reinterpret_cast<_Unwind_Context*>(&cursor));
    switch (personalityResult) {
      case _URC_HANDLER_FOUND:
        break;
      case _URC_CONTINUE_UNWIND:
        continue;
      default:
        break;
    }

    char proc_name[512];
    unw_word_t offset;
    if (unw_get_proc_name(&cursor, proc_name, sizeof(proc_name), &offset) !=
        UNW_ESUCCESS) {
      // The symbol has no name, so see if it belongs to the same image as
      // this function.
      Dl_info dl_info;
      if (dladdr(reinterpret_cast<const void*>(frame_info.start_ip),
                 &dl_info)) {
        if (dl_info.dli_fbase == this_base_address) {
          // This is a handler in our image, so allow it to run.
          handler_found = true;
          break;
        }
      }

      // This handler does not belong to us, so continue the search.
      continue;
    }

    // Check if the function is one that is known to obscure (by way of
    // catch-and-rethrow) exception stack traces. If it is, sinkhole it
    // by crashing here at the point of throw.
    static constexpr const char* kExceptionSymbolNameSinkholes[] = {
        // The two CF symbol names will also be captured by the CoreFoundation
        // library path check below, but for completeness they are listed here,
        // since they appear unredacted.
        "CFRunLoopRunSpecific",
        "_CFXNotificationPost",
        "__NSFireDelayedPerform",
    };
    for (const char* sinkhole : kExceptionSymbolNameSinkholes) {
      if (strcmp(sinkhole, proc_name) == 0) {
        TerminatingFromUncaughtNSException(exception, sinkhole);
      }
    }

    // On iOS, function names are often reported as "<redacted>", although they
    // do appear when attached to the debugger.  When this happens, use the path
    // of the image to determine if the handler is an exception sinkhole.
    static constexpr const char* kExceptionLibraryPathSinkholes[] = {
        // Everything in this library is a sinkhole, specifically
        // _dispatch_client_callout.  Both are needed here depending on whether
        // the debugger is attached (introspection only appears when a simulator
        // is attached to a debugger).
        "/usr/lib/system/introspection/libdispatch.dylib",
        "/usr/lib/system/libdispatch.dylib",

        // __CFRunLoopDoTimers and __CFRunLoopRun are sinkholes. Consider also
        // checking that a few frames up is CFRunLoopRunSpecific().
        "/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation",
    };

    Dl_info dl_info;
    if (dladdr(reinterpret_cast<const void*>(frame_info.start_ip), &dl_info) !=
        0) {
      for (const char* sinkhole : kExceptionLibraryPathSinkholes) {
        if (ModulePathMatchesSinkhole(dl_info.dli_fname, sinkhole)) {
          TerminatingFromUncaughtNSException(exception, sinkhole);
        }
      }
    }

    // Some <redacted> sinkholes are harder to find. _UIGestureEnvironmentUpdate
    // in UIKitCore is an example. UIKitCore can't be added to
    // kExceptionLibraryPathSinkholes because it uses Objective-C exceptions
    // internally and also has has non-sinkhole handlers. While all the
    // calling methods in UIKit are marked <redacted> starting in iOS14, it's
    // currently true that all callers to _UIGestureEnvironmentUpdate are within
    // UIGestureEnvironment.  That means a very hacky way to detect this are to
    // check if the calling method IMP is within the range of all
    // UIGestureEnvironment methods.
    static constexpr const char kUIKitCorePath[] =
        "/System/Library/PrivateFrameworks/UIKitCore.framework/UIKitCore";
    if (ModulePathMatchesSinkhole(dl_info.dli_fname, kUIKitCorePath)) {
      unw_proc_info_t caller_frame_info;
      if (LoggingUnwStep(&cursor) > 0 &&
          unw_get_proc_info(&cursor, &caller_frame_info) == UNW_ESUCCESS) {
        auto uigestureimp_lambda = [](IMP* max) {
          IMP min = *max = bit_cast<IMP>(nullptr);
          unsigned int method_count = 0;
          std::unique_ptr<Method[], base::FreeDeleter> method_list(
              class_copyMethodList(NSClassFromString(@"UIGestureEnvironment"),
                                   &method_count));
          if (method_count > 0) {
            min = *max = method_getImplementation(method_list[0]);
            for (unsigned int method_index = 1; method_index < method_count;
                 method_index++) {
              IMP method_imp =
                  method_getImplementation(method_list[method_index]);
              *max = std::max(method_imp, *max);
              min = std::min(method_imp, min);
            }
          }
          return min;
        };

        static IMP gesture_environment_max_imp;
        static IMP gesture_environment_min_imp =
            uigestureimp_lambda(&gesture_environment_max_imp);

        IMP caller = reinterpret_cast<IMP>(caller_frame_info.start_ip);
        if (gesture_environment_min_imp && gesture_environment_max_imp &&
            caller >= gesture_environment_min_imp &&
            caller <= gesture_environment_max_imp) {
          TerminatingFromUncaughtNSException(exception,
                                             "_UIGestureEnvironmentUpdate");
        }
      }
    }

    handler_found = true;

    break;
  }

  // If no handler is found, __cxa_throw would call failed_throw and terminate.
  // See:
  // https://github.com/llvm/llvm-project/blob/c5d2746fbea7/libcxxabi/src/cxa_exception.cpp
  // __cxa_throw. Instead, terminate via TerminatingFromUncaughtNSException so
  // the exception name and reason are properly recorded.
  if (!handler_found) {
    TerminatingFromUncaughtNSException(exception, "__cxa_throw");
  }

  // Forward to the next preprocessor.
  if (g_next_preprocessor)
    return g_next_preprocessor(exception);

  return exception;
}

}  // namespace

namespace crashpad {

void InstallObjcExceptionPreprocessor() {
  DCHECK(!g_exception_preprocessor_installed);

  g_next_preprocessor =
      objc_setExceptionPreprocessor(&ObjcExceptionPreprocessor);
  g_exception_preprocessor_installed = true;
}

void UninstallObjcExceptionPreprocessor() {
  DCHECK(g_exception_preprocessor_installed);

  objc_setExceptionPreprocessor(g_next_preprocessor);
  g_next_preprocessor = nullptr;
  g_exception_preprocessor_installed = false;
}

}  // namespace crashpad
