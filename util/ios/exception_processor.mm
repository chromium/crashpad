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
#include <cxxabi.h>
#include <dlfcn.h>
#include <libunwind.h>
#include <mach-o/loader.h>
#include <objc/objc-exception.h>
#include <objc/objc.h>
#include <objc/runtime.h>
#include <stdint.h>
#include <sys/types.h>
#include <unwind.h>
#include <exception>
#include <typeinfo>

#include <type_traits>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"

namespace {

// From 10.15.0 objc4-779.1/runtime/objc-exception.mm
struct objc_typeinfo {
  const void** vtable;
  const char* name;
  Class cls_unremapped;
};
struct objc_exception {
  id obj;
  objc_typeinfo tinfo;
};

// From 10.15.0 objc4-779.1/runtime/objc-abi.h
extern "C" const void* objc_ehtype_vtable[];

// https://github.com/llvm/llvm-project/blob/e6a39f00e8d0cd3684df54fb03d288efe2969202/libcxxabi/src/cxa_exception.h
static const uint64_t kOurExceptionClass = 0x434C4E47432B2B00;

struct __cxa_exception {
#ifdef ARCH_CPU_64_BITS
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

objc_exception_preprocessor g_next_preprocessor = nullptr;

bool g_exception_preprocessor_installed = false;

constexpr const char* kExceptionSymbolNameSinkholes[] = {
    "CFRunLoopRunSpecific",
    "_CFXNotificationPost",
    "__NSFireDelayedPerform",
};

// Use library paths when symbol names are redacted.
constexpr const char* kExceptionLibraryPathSinkholes[] = {
// Everything in this library is a siknhole, specifically
// _dispatch_client_callout.
#if defined(ARCH_CPU_X86_64)
    "/usr/lib/system/introspection/libdispatch.dylib",
#elif defined(ARCH_CPU_ARM64)
    "/usr/lib/system/libdispatch.dylib",
#endif

    // __CFRunLoopDoTimers and __CFRunLoopRun are sinkholes. Consider also
    // checking that a few frames up is CFRunLoopRunSpecific().
    "/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation"};

// This function is used to make it clear to the crash processor that this is
// a forced exception crash.
void TERMINATING_FROM_UNCAUGHT_NSEXCEPTION(id exception, const char* sinkhole) {
  // TODO(justincohen): This is incomplete, as the signal handler will not have
  // access to the exception name and reason.  Pass that along somehow here.
  NSString* exception_message_ns = [NSString
      stringWithFormat:@"%@: %@", [exception name], [exception reason]];
  std::string exception_message = base::SysNSStringToUTF8(exception_message_ns);
  LOG(FATAL) << "Terminating from Objective-C exception: " << exception_message
             << " with sinkhole: " << sinkhole;
}

id ObjcExceptionPreprocessor(id exception) {
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

  static const mach_header_64* this_base_address = []() {
    Dl_info dl_info;
    dl_info.dli_fbase = nullptr;
    if (!dladdr(reinterpret_cast<const void*>(&ObjcExceptionPreprocessor),
                &dl_info)) {
      LOG(ERROR) << "dladdr: " << dlerror();
    }
    return reinterpret_cast<const mach_header_64*>(dl_info.dli_fbase);
  }();

  // Generate an exception_header for the __personality_routine.
  // From 10.15.0 objc4-779.1/runtime/objc-exception.mm::objc_exception_throw
  objc_exception* exc = reinterpret_cast<objc_exception*>(
      __cxxabiv1::__cxa_allocate_exception(sizeof(objc_exception)));
  exc->obj = exception;
  exc->tinfo.vtable = objc_ehtype_vtable + 2;
  exc->tinfo.name = object_getClassName(exception);
  exc->tinfo.cls_unremapped = object_getClass(exception);
  // https://github.com/llvm/llvm-project/blob/e6a39f00e8d0cd3684df54fb03d288efe2969202/libcxxabi/src/cxa_exception.cpp::__cxa_throw
  __cxa_exception* exception_header =
      reinterpret_cast<__cxa_exception*>(exc) - 1;
  exception_header->unexpectedHandler = std::get_unexpected();
  exception_header->terminateHandler = std::get_terminate();
  exception_header->exceptionType =
      reinterpret_cast<std::type_info*>(&exc->tinfo);
  exception_header->unwindHeader.exception_class = kOurExceptionClass;

  bool handler_found = false;
  while (unw_step(&cursor) > 0) {
    unw_proc_info_t frame_info;
    if (unw_get_proc_info(&cursor, &frame_info) != UNW_ESUCCESS) {
      continue;
    }

    if (frame_info.handler == 0)
      continue;

    // Check to see if the handler is really an exception handler.
    __personality_routine p =
        reinterpret_cast<__personality_routine>(frame_info.handler);
    // From 10.15.0 libunwind-35.4/src/UnwindLevel1.c
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

    char proc_name[64];
    unw_word_t offset;
    if (unw_get_proc_name(&cursor, proc_name, sizeof(proc_name), &offset) !=
        UNW_ESUCCESS) {
      // The symbol has no name, so see if it belongs to the same image as
      // this function.
      Dl_info dl_info;
      if (dladdr(reinterpret_cast<const void*>(frame_info.start_ip),
                 &dl_info) != 0) {
        if (dl_info.dli_fbase == this_base_address) {
          // This is a handler in our image, so allow it to run.
          break;
        }
      }

      // This handler does not belong to us, so continue the search.
      continue;
    }

    // Check if the function is one that is known to obscure (by way of
    // catch-and-rethrow) exception stack traces. If it is, sinkhole it
    // by crashing here at the point of throw.
    for (const char* sinkhole : kExceptionSymbolNameSinkholes) {
      if (strcmp(sinkhole, proc_name) == 0) {
        TERMINATING_FROM_UNCAUGHT_NSEXCEPTION(exception, sinkhole);
      }
    }

    // The symbol has no name, so see if it belongs to the same image as
    // this function.
    Dl_info dl_info;
    if (dladdr(reinterpret_cast<const void*>(frame_info.start_ip), &dl_info) !=
        0) {
      std::string library_path(dl_info.dli_fname);
      for (const char* sinkhole : kExceptionLibraryPathSinkholes) {
        if (library_path.find(sinkhole) != std::string::npos) {
          TERMINATING_FROM_UNCAUGHT_NSEXCEPTION(exception, sinkhole);
        }
      }
    }

    handler_found = true;

    break;
  }

  // If no handler is found, __cxa_throw will call failed_throw and terminate.
  // https://github.com/llvm/llvm-project/blob/e6a39f00e8d0cd3684df54fb03d288efe2969202/libcxxabi/src/cxa_exception.cpp::__cxa_throw
  if (!handler_found) {
    TERMINATING_FROM_UNCAUGHT_NSEXCEPTION(exception, "__cxa_throw");
  }

  // Forward to the next preprocessor.
  if (g_next_preprocessor)
    return g_next_preprocessor(exception);

  return exception;
}

}  // namespace

namespace crashpad {

void InstallObjcExceptionPreprocessor() {
  if (g_exception_preprocessor_installed)
    return;

  g_next_preprocessor =
      objc_setExceptionPreprocessor(&ObjcExceptionPreprocessor);
  g_exception_preprocessor_installed = true;
}

void UninstallObjcExceptionPreprocessor() {
  objc_setExceptionPreprocessor(g_next_preprocessor);
  g_next_preprocessor = nullptr;
  g_exception_preprocessor_installed = false;
}

}  // namespace crashpad
