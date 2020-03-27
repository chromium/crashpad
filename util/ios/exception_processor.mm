// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/ios/exception_processor.h"

#import <Foundation/Foundation.h>
#include <dlfcn.h>
#include <libunwind.h>
#include <objc/objc-exception.h>

#include <type_traits>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"

namespace crashpad {

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
  NSString* exception_message_ns = [NSString
      stringWithFormat:@"%@: %@", [exception name], [exception reason]];
  std::string exception_message = base::SysNSStringToUTF8(exception_message_ns);

  //  static crash_reporter::CrashKeyString<256> crash_key("nsexception");
  //  crash_key.Set(exception_message);

  LOG(FATAL) << "Terminating from Objective-C exception: " << exception_message;
}

typedef int _Unwind_Action;
enum : _Unwind_Action {
  _UA_SEARCH_PHASE = 1,
  _UA_CLEANUP_PHASE = 2,
  _UA_HANDLER_FRAME = 4,
  _UA_FORCE_UNWIND = 8
};

typedef int _Unwind_Reason_Code;
enum : _Unwind_Reason_Code {
  _URC_NO_REASON = 0,
  _URC_FOREIGN_EXCEPTION_CAUGHT = 1,
  _URC_FATAL_PHASE2_ERROR = 2,
  _URC_FATAL_PHASE1_ERROR = 3,
  _URC_NORMAL_STOP = 4,
  _URC_END_OF_STACK = 5,
  _URC_HANDLER_FOUND = 6,
  _URC_INSTALL_CONTEXT = 7,
  _URC_CONTINUE_UNWIND = 8
};


typedef _Unwind_Reason_Code (*__personality_routine)
      (int version,
       _Unwind_Action actions,
       uint64_t exceptionClass,
       struct _Unwind_Exception* exceptionObject,
       struct _Unwind_Context* context);
     


struct frame_ips {
  uintptr_t start;
  uintptr_t end;
};
struct frame_range {
  uintptr_t ip_start;
  uintptr_t ip_end;
  uintptr_t cfa;
  // precise ranges within ip_start..ip_end; nil or {0,0} terminated
  frame_ips* ips;
};

struct dwarf_eh_bases {
  uintptr_t tbase;
  uintptr_t dbase;
  uintptr_t func;
};

extern "C" _Unwind_Reason_Code __gxx_personality_v0(int,
                                                    _Unwind_Action,
                                                    uint64_t,
                                                    struct _Unwind_Exception*,
                                                    struct _Unwind_Context*);

extern "C" _Unwind_Reason_Code __objc_personality_v0(int,
                                                     _Unwind_Action,
                                                     uint64_t,
                                                     struct _Unwind_Exception*,
                                                     struct _Unwind_Context*);

// Dwarf eh data encodings
#define DW_EH_PE_omit 0xff  // no data follows

#define DW_EH_PE_absptr 0x00
#define DW_EH_PE_uleb128 0x01
#define DW_EH_PE_udata2 0x02
#define DW_EH_PE_udata4 0x03
#define DW_EH_PE_udata8 0x04
#define DW_EH_PE_sleb128 0x09
#define DW_EH_PE_sdata2 0x0A
#define DW_EH_PE_sdata4 0x0B
#define DW_EH_PE_sdata8 0x0C

#define DW_EH_PE_pcrel 0x10
#define DW_EH_PE_textrel 0x20
#define DW_EH_PE_datarel 0x30
#define DW_EH_PE_funcrel 0x40
#define DW_EH_PE_aligned 0x50  // fixme

#define DW_EH_PE_indirect 0x80  // gcc extension
#if 0

/***********************************************************************
 * read_uleb
 * Read a LEB-encoded unsigned integer from the address stored in *pp.
 * Increments *pp past the bytes read.
 * Adapted from DWARF Debugging Information Format 1.1, appendix 4
 **********************************************************************/
static uintptr_t read_uleb(uintptr_t* pp) {
  uintptr_t result = 0;
  uintptr_t shift = 0;
  unsigned char byte;
  do {
    byte = *(const unsigned char*)(*pp)++;
    result |= (byte & 0x7f) << shift;
    shift += 7;
  } while (byte & 0x80);
  return result;
}

/***********************************************************************
 * read_sleb
 * Read a LEB-encoded signed integer from the address stored in *pp.
 * Increments *pp past the bytes read.
 * Adapted from DWARF Debugging Information Format 1.1, appendix 4
 **********************************************************************/
static intptr_t read_sleb(uintptr_t* pp) {
  uintptr_t result = 0;
  uintptr_t shift = 0;
  unsigned char byte;
  do {
    byte = *(const unsigned char*)(*pp)++;
    result |= (byte & 0x7f) << shift;
    shift += 7;
  } while (byte & 0x80);
  if ((shift < 8 * sizeof(intptr_t)) && (byte & 0x40)) {
    result |= ((intptr_t)-1) << shift;
  }
  return result;
}

/***********************************************************************
 * read_address
 * Reads an encoded address from the address stored in *pp.
 * Increments *pp past the bytes read.
 * The data is interpreted according to the given dwarf encoding
 * and base addresses.
 **********************************************************************/
static uintptr_t read_address(uintptr_t* pp,
                              const struct dwarf_eh_bases* bases,
                              unsigned char encoding) {
  uintptr_t result = 0;
  uintptr_t oldp = *pp;

  // fixme need DW_EH_PE_aligned?

#define READ(type)        \
  result = *(type*)(*pp); \
  *pp += sizeof(type);

  if (encoding == DW_EH_PE_omit)
    return 0;

  switch (encoding & 0x0f) {
    case DW_EH_PE_absptr:
      READ(uintptr_t);
      break;
    case DW_EH_PE_uleb128:
      result = read_uleb(pp);
      break;
    case DW_EH_PE_udata2:
      READ(uint16_t);
      break;
    case DW_EH_PE_udata4:
      READ(uint32_t);
      break;
#if __LP64__
    case DW_EH_PE_udata8:
      READ(uint64_t);
      break;
#endif
    case DW_EH_PE_sleb128:
      result = read_sleb(pp);
      break;
    case DW_EH_PE_sdata2:
      READ(int16_t);
      break;
    case DW_EH_PE_sdata4:
      READ(int32_t);
      break;
#if __LP64__
    case DW_EH_PE_sdata8:
      READ(int64_t);
      break;
#endif
    default:
      //        _objc_inform("unknown DWARF EH encoding 0x%x at %p",
      //                     encoding, (void *)*pp);
      break;
  }

#undef READ

  if (result) {
    switch (encoding & 0x70) {
      case DW_EH_PE_pcrel:
        // fixme correct?
        result += (uintptr_t)oldp;
        break;
      case DW_EH_PE_textrel:
        result += bases->tbase;
        break;
      case DW_EH_PE_datarel:
        result += bases->dbase;
        break;
      case DW_EH_PE_funcrel:
        result += bases->func;
        break;
      case DW_EH_PE_aligned:
        //            _objc_inform("unknown DWARF EH encoding 0x%x at %p",
        //                         encoding, (void *)*pp);
        break;
      default:
        // no adjustment
        break;
    }

    if (encoding & DW_EH_PE_indirect) {
      result = *(uintptr_t*)result;
    }
  }

  return (uintptr_t)result;
}

static bool isObjCExceptionCatcher(uintptr_t lsda,
                                   uintptr_t ip,
                                   const struct dwarf_eh_bases* bases,
                                   struct frame_range* frame) {
  unsigned char LPStart_enc = *(const unsigned char*)lsda++;

  if (LPStart_enc != DW_EH_PE_omit) {
    read_address(&lsda, bases, LPStart_enc);  // LPStart
  }

  unsigned char TType_enc = *(const unsigned char*)lsda++;
  if (TType_enc != DW_EH_PE_omit) {
    read_uleb(&lsda);  // TType
  }

  unsigned char call_site_enc = *(const unsigned char*)lsda++;
  uintptr_t length = read_uleb(&lsda);
  uintptr_t call_site_table = lsda;
  uintptr_t call_site_table_end = call_site_table + length;
  uintptr_t action_record_table = call_site_table_end;

  uintptr_t action_record = 0;
  uintptr_t p = call_site_table;

  uintptr_t try_start;
  uintptr_t try_end;
  uintptr_t try_landing_pad;

  while (p < call_site_table_end) {
    uintptr_t start = read_address(&p, bases, call_site_enc) + bases->func;
    uintptr_t len = read_address(&p, bases, call_site_enc);
    uintptr_t pad = read_address(&p, bases, call_site_enc);
    uintptr_t action = read_uleb(&p);

    if (ip < start) {
      // no more source ranges
      return false;
    } else if (ip < start + len) {
      // found the range
      if (!pad)
        return false;  // ...but it has no landing pad
      // found the landing pad
      action_record = action ? action_record_table + action - 1 : 0;
      try_start = start;
      try_end = start + len;
      try_landing_pad = pad;
      break;
    }
  }

  if (!action_record)
    return false;  // no catch handlers

  // has handlers, destructors, and/or throws specifications
  // Use this frame if it has any handlers
  bool has_handler = false;
  p = action_record;
  intptr_t offset;
  do {
    intptr_t filter = read_sleb(&p);
    uintptr_t temp = p;
    offset = read_sleb(&temp);
    p += offset;

    if (filter < 0) {
      // throws specification - ignore
    } else if (filter == 0) {
      // destructor - ignore
    } else /* filter >= 0 */ {
      // catch handler - use this frame
      has_handler = true;
      break;
    }
  } while (offset);

  if (!has_handler)
    return false;

  // Count the number of s  ource ranges with the same landing pad as our match
  unsigned int range_count = 0;
  p = call_site_table;
  while (p < call_site_table_end) {
    /*start*/ read_address(&p, bases, call_site_enc) /*+bases->func*/;
    /*len*/ read_address(&p, bases, call_site_enc);
    uintptr_t pad = read_address(&p, bases, call_site_enc);
    /*action*/ read_uleb(&p);

    if (pad == try_landing_pad) {
      range_count++;
    }
  }

  if (range_count == 1) {
    // No other source ranges with the same landing pad. We're done here.
    frame->ips = nil;
  } else {
    // Record all ranges with the same landing pad as our match.
    frame->ips = (frame_ips*)malloc((range_count + 1) * sizeof(frame->ips[0]));
    unsigned int r = 0;
    p = call_site_table;
    while (p < call_site_table_end) {
      uintptr_t start = read_address(&p, bases, call_site_enc) + bases->func;
      uintptr_t len = read_address(&p, bases, call_site_enc);
      uintptr_t pad = read_address(&p, bases, call_site_enc);
      /*action*/ read_uleb(&p);

      if (pad == try_landing_pad) {
        if (start < try_start)
          try_start = start;
        if (start + len > try_end)
          try_end = start + len;
        frame->ips[r].start = start;
        frame->ips[r].end = start + len;
        r++;
      }
    }

    frame->ips[r].start = 0;
    frame->ips[r].end = 0;
  }

  frame->ip_start = try_start;
  frame->ip_end = try_end;

  return true;
}
#endif

#define __ptrauth_cxx_vtable_pointer

struct objc_typeinfo {
    // Position of vtable and name fields must match C++ typeinfo object
    const void ** __ptrauth_cxx_vtable_pointer vtable;  // objc_ehtype_vtable+2
    const char *name;     // c++ typeinfo string

    Class cls_unremapped;
};

struct objc_exception {
    id obj;
    struct objc_typeinfo tinfo;
};

OBJC_EXTERN void *__cxa_allocate_exception(size_t thrown_size);

OBJC_EXPORT const void * _Nullable objc_ehtype_vtable[];

static id ObjcExceptionPreprocessor(id exception) {
#if 0
  {
    unw_context_t context;
    unw_getcontext(&context);

    unw_cursor_t cursor1;
    unw_init_local(&cursor1, &context);
    
    struct objc_exception *exc = (struct objc_exception *)
        __cxa_allocate_exception(sizeof(struct objc_exception));
//    exc->obj = exception;
//    exc->tinfo.vtable = objc_ehtype_vtable+2;
//    exc->tinfo.name = object_getClassName(exception);
//    exc->tinfo.cls_unremapped = exception ? exception->getIsa() : Nil;
    
    // walk each frame looking for a place to stop
    for (bool handlerNotFound = true; handlerNotFound; ) {

      // ask libuwind to get next frame (skip over first which is _Unwind_RaiseException)
      int stepResult = unw_step(&cursor1);
      if ( stepResult == 0 ) {
//        DEBUG_PRINT_UNWINDING("unwind_phase1(ex_ojb=%p): unw_step() reached bottom => _URC_END_OF_STACK\n", exception);
//        return _URC_END_OF_STACK;
        return g_next_preprocessor(exception);
      }
      else if ( stepResult < 0 ) {
//        DEBUG_PRINT_UNWINDING("unwind_phase1(ex_ojb=%p): unw_step failed => _URC_FATAL_PHASE1_ERROR\n", exception);
//        return _URC_FATAL_PHASE1_ERROR;
        return g_next_preprocessor(exception);
      }
      
      // see if frame has code to run (has personality routine)
      unw_proc_info_t frameInfo;
//      unw_word_t sp;
      if ( unw_get_proc_info(&cursor1, &frameInfo) != UNW_ESUCCESS ) {
//        DEBUG_PRINT_UNWINDING("unwind_phase1(ex_ojb=%p): unw_get_proc_info failed => _URC_FATAL_PHASE1_ERROR\n", exception);
//        return _URC_FATAL_PHASE1_ERROR;
        return g_next_preprocessor(exception);
      }
//
//      // debugging
//      if ( DEBUG_PRINT_UNWINDING_TEST ) {
//        char functionName[512];
//        unw_word_t  offset;
//        if ( (unw_get_proc_name(&cursor1, functionName, 512, &offset) != UNW_ESUCCESS) || (frameInfo.start_ip+offset > frameInfo.end_ip) )
//          strcpy(functionName, ".anonymous.");
//        unw_word_t pc;
//        unw_get_reg(&cursor1, UNW_REG_IP, &pc);
//        DEBUG_PRINT_UNWINDING("unwind_phase1(ex_ojb=%p): pc=0x%llX, start_ip=0x%llX, func=%s, lsda=0x%llX, personality=0x%llX\n",
//                exception, pc, frameInfo.start_ip, functionName, frameInfo.lsda, frameInfo.handler);
//      }
//
      // if there is a personality routine, ask it if it will want to stop at this frame
      if ( frameInfo.handler != 0 ) {
        __personality_routine p = (__personality_routine)(long)(frameInfo.handler);
//        DEBUG_PRINT_UNWINDING("unwind_phase1(ex_ojb=%p): calling personality function %p\n", exception, p);
      
        
        _Unwind_Reason_Code personalityResult = (*p)(1, _UA_SEARCH_PHASE,
              0, (struct _Unwind_Exception*)exc,
              (struct _Unwind_Context*)(&cursor1));
        switch ( personalityResult ) {
          case _URC_HANDLER_FOUND:
            // found a catch clause or locals that need destructing in this frame
            // stop search and remember stack pointer at the frame
            handlerNotFound = false;
//            unw_get_reg(&cursor1, UNW_REG_SP, &sp);
//            exception_object->private_2 = sp;
//            DEBUG_PRINT_UNWINDING("unwind_phase1(ex_ojb=%p): _URC_HANDLER_FOUND\n", exception_object);
//            return _URC_NO_REASON;
            return g_next_preprocessor(exception);
            
          case _URC_CONTINUE_UNWIND:
//            DEBUG_PRINT_UNWINDING("unwind_phase1(ex_ojb=%p): _URC_CONTINUE_UNWIND\n", exception_object);
            // continue unwinding
            break;
            
          default:
            // something went wrong
            return g_next_preprocessor(exception);
//            DEBUG_PRINT_UNWINDING("unwind_phase1(ex_ojb=%p): _URC_FATAL_PHASE1_ERROR\n", exception_object);
//            return _URC_FATAL_PHASE1_ERROR;
        }
      }
    }
    return g_next_preprocessor(exception);
  }
#endif
  
  // Unwind the stack looking for any exception handlers. If an exception
  // handler is encountered, test to see if it is a function known to catch-
  // and-rethrow as a "top-level" exception handler. Various routines in
  // Cocoa do this, and it obscures the crashing stack, since the original
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

  struct objc_exception *exc = (struct objc_exception *)
      __cxa_allocate_exception(sizeof(struct objc_exception));

  while (unw_step(&cursor) > 0) {
    unw_proc_info_t frame_info;
    if (unw_get_proc_info(&cursor, &frame_info) != UNW_ESUCCESS) {
      continue;
    }

    // This frame has an exception handler.
    if (frame_info.handler != 0) {
      
      __personality_routine p = (__personality_routine)(long)(frame_info.handler);
      _Unwind_Reason_Code personalityResult = (*p)(1, _UA_SEARCH_PHASE,
        0, (struct _Unwind_Exception*)exc, (struct _Unwind_Context*)(&cursor));
      switch ( personalityResult ) {
        case _URC_HANDLER_FOUND:
          break;
        case _URC_CONTINUE_UNWIND:
          continue;
        default:
          break;
      }

//
//      // This frame is an objc handler, lets do some extra work.
//      if (frame_info.handler == (uintptr_t)__objc_personality_v0) {
//        // must have landing pad
//        if (frame_info.lsda == 0)
//          continue;
//        // must have landing pad that catches objc exceptions
//        struct dwarf_eh_bases bases;
//        bases.tbase = 0;  // from unwind-dw2-fde-darwin.c:examine_objects()
//        bases.dbase = 0;  // from unwind-dw2-fde-darwin.c:examine_objects()
//        bases.func = frame_info.start_ip;
//        unw_word_t reg_ip;
//        unw_get_reg(&cursor, UNW_REG_IP, &reg_ip);
//        reg_ip -= 1;
//        struct frame_range try_range = {0, 0, 0, 0};
//
//        if (!isObjCExceptionCatcher(
//                frame_info.lsda, reg_ip, &bases, &try_range))
//          continue;
//      }
//
//      if (frame_info.handler == (uintptr_t)__gxx_personality_v0) {
//        // What do I do here?
//        continue;
//      }

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
