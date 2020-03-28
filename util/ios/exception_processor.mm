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
#include <dlfcn.h>
#include <libunwind.h>
#include <objc/objc-exception.h>
#include <objc/runtime.h>
#include <unwind.h>

#include <type_traits>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"

namespace crashpad {

// From objc4-779.1/runtime/objc-exception.mm
OBJC_EXTERN void* __cxa_allocate_exception(size_t thrown_size);
struct objc_typeinfo {
  // Position of vtable and name fields must match C++ typeinfo object
  const void** vtable;  // objc_ehtype_vtable+2
  const char* name;  // c++ typeinfo string

  Class cls_unremapped;
};
struct objc_exception {
  id obj;
  struct objc_typeinfo tinfo;
};

// From objc4-779.1/runtime/objc-abi.h
OBJC_EXPORT const void* _Nullable objc_ehtype_vtable[];

// From github.com/llvm/llvm-project/blob/master/libcxxabi/src/cxa_exception.h
static const uint64_t kOurExceptionClass = 0x434C4E47432B2B00;
struct __cxa_exception {
#if defined(__LP64__) || defined(_WIN64) || defined(_LIBCXXABI_ARM_EHABI)
  // Now _Unwind_Exception is marked with __attribute__((aligned)),
  // which implies __cxa_exception is also aligned. Insert padding
  // in the beginning of the struct, rather than before unwindHeader.
  void* reserve;

  // This is a new field to support C++ 0x exception_ptr.
  // For binary compatibility it is at the start of this
  // struct which is prepended to the object thrown in
  // __cxa_allocate_exception.
  size_t referenceCount;
#endif

  //  Manage the exception object itself.
  std::type_info* exceptionType;
  void (*exceptionDestructor)(void*);
  std::unexpected_handler unexpectedHandler;
  std::terminate_handler terminateHandler;

  __cxa_exception* nextException;

  int handlerCount;

#if defined(_LIBCXXABI_ARM_EHABI)
  __cxa_exception* nextPropagatingException;
  int propagationCount;
#else
  int handlerSwitchValue;
  const unsigned char* actionRecord;
  const unsigned char* languageSpecificData;
  void* catchTemp;
  void* adjustedPtr;
#endif

#if !defined(__LP64__) && !defined(_WIN64) && !defined(_LIBCXXABI_ARM_EHABI)
  // This is a new field to support C++ 0x exception_ptr.
  // For binary compatibility it is placed where the compiler
  // previously adding padded to 64-bit align unwindHeader.
  size_t referenceCount;
#endif
  _Unwind_Exception unwindHeader;
};

static objc_exception_preprocessor g_next_preprocessor = nullptr;

static const char* const kExceptionSinkholes[] = {
    "CFRunLoopRunSpecific",
    "_CFXNotificationPost",
    "__CFRunLoopDoTimers",
    "__CFRunLoopRun",
    "__NSFireDelayedPerform",
    "_dispatch_client_callout",
};

// This function is used to make it clear to the crash processor that this is
// a forced exception crash.
static NOINLINE void TERMINATING_FROM_UNCAUGHT_NSEXCEPTION(id exception) {
  // TODO(justincohen): This is incomplete, as the signal handler will not have
  // access to the exception name and reason.  Pass that along somehow here.
  NSString* exception_message_ns = [NSString
      stringWithFormat:@"%@: %@", [exception name], [exception reason]];
  std::string exception_message = base::SysNSStringToUTF8(exception_message_ns);
  LOG(FATAL) << "Terminating from Objective-C exception: " << exception_message;
}

static id ObjcExceptionPreprocessor(id exception) {
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

  // Get the base address for the image that contains this function.
  Dl_info dl_info;
  const void* this_base_address = 0;
  if (dladdr(reinterpret_cast<const void*>(&ObjcExceptionPreprocessor),
             &dl_info) != 0) {
    this_base_address = dl_info.dli_fbase;
  }

  // Generate an exception_header for the __personality_routine.
  // From objc4-779.1/runtime/objc-exception.mm::objc_exception_throw
  struct objc_exception* exc = (struct objc_exception*)__cxa_allocate_exception(
      sizeof(struct objc_exception));
  exc->obj = exception;
  exc->tinfo.vtable = objc_ehtype_vtable + 2;
  exc->tinfo.name = object_getClassName(exception);
  exc->tinfo.cls_unremapped = object_getClass(exception);
  // From github.com/llvm/llvm-project/blob/master/libcxxabi/src/
  // cxa_exception.cpp::__cxa_throw
  __cxa_exception* exception_header =
      reinterpret_cast<__cxa_exception*>(exc) - 1;
  exception_header->unexpectedHandler = std::get_unexpected();
  exception_header->terminateHandler = std::get_terminate();
  exception_header->exceptionType = (std::type_info*)&exc->tinfo;
  exception_header->unwindHeader.exception_class = kOurExceptionClass;

  while (unw_step(&cursor) > 0) {
    unw_proc_info_t frame_info;
    if (unw_get_proc_info(&cursor, &frame_info) != UNW_ESUCCESS) {
      continue;
    }

    if (frame_info.handler == 0)
      continue;

    // Check to see if the handler is really an exception handler.
    __personality_routine p = (__personality_routine)(long)(frame_info.handler);
    // From libunwind-35.4/src/UnwindLevel1.c
    _Unwind_Reason_Code personalityResult =
        (*p)(1,
             _UA_SEARCH_PHASE,
             exception_header->unwindHeader.exception_class,
             (_Unwind_Exception*)&exception_header->unwindHeader,
             (_Unwind_Context*)(&cursor));
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
    for (const char* sinkhole : kExceptionSinkholes) {
      if (strcmp(sinkhole, proc_name) == 0) {
        TERMINATING_FROM_UNCAUGHT_NSEXCEPTION(exception);
      }
    }

    VLOG(1) << "Stopping search for exception handler at " << proc_name;

    break;
  }

  // Forward to the next preprocessor.
  if (g_next_preprocessor)
    return g_next_preprocessor(exception);

  return exception;
}

void InstallObjcExceptionPreprocessor() {
  if (g_next_preprocessor)
    return;
  g_next_preprocessor =
      objc_setExceptionPreprocessor(&ObjcExceptionPreprocessor);
}

void UninstallObjcExceptionPreprocessor() {
  objc_setExceptionPreprocessor(g_next_preprocessor);
  g_next_preprocessor = nullptr;
}

}  // namespace crashpad
