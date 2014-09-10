// Copyright 2014 The Crashpad Authors. All rights reserved.
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

#include "util/mach/exc_server_variants.h"

#include <algorithm>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "util/mach/exc.h"
#include "util/mach/excServer.h"
#include "util/mach/mach_exc.h"
#include "util/mach/mach_excServer.h"

extern "C" {

// These six functions are not used, and are in fact obsoleted by the other
// functionality implemented in this file. The standard MIG-generated exc_server
// (in excServer.c) and mach_exc_server (in mach_excServer.c) server dispatch
// routines usable with the standard mach_msg_server() function call out to
// these functions. exc_server() and mach_exc_server() are unused and are
// replaced by the more flexible ExcServer and MachExcServer, but the linker
// still needs to see these six function definitions.

kern_return_t catch_exception_raise(exception_handler_t exception_port,
                                    thread_t thread,
                                    task_t task,
                                    exception_type_t exception,
                                    exception_data_t code,
                                    mach_msg_type_number_t code_count) {
  NOTREACHED();
  return KERN_FAILURE;
}

kern_return_t catch_exception_raise_state(
    exception_handler_t exception_port,
    exception_type_t exception,
    exception_data_t code,
    mach_msg_type_number_t code_count,
    thread_state_flavor_t* flavor,
    thread_state_t old_state,
    mach_msg_type_number_t old_state_count,
    thread_state_t new_state,
    mach_msg_type_number_t* new_state_count) {
  NOTREACHED();
  return KERN_FAILURE;
}

kern_return_t catch_exception_raise_state_identity(
    exception_handler_t exception_port,
    thread_t thread,
    task_t task,
    exception_type_t exception,
    exception_data_t code,
    mach_msg_type_number_t code_count,
    thread_state_flavor_t* flavor,
    thread_state_t old_state,
    mach_msg_type_number_t old_state_count,
    thread_state_t new_state,
    mach_msg_type_number_t* new_state_count) {
  NOTREACHED();
  return KERN_FAILURE;
}

kern_return_t catch_mach_exception_raise(exception_handler_t exception_port,
                                         thread_t thread,
                                         task_t task,
                                         exception_type_t exception,
                                         mach_exception_data_t code,
                                         mach_msg_type_number_t code_count) {
  NOTREACHED();
  return KERN_FAILURE;
}

kern_return_t catch_mach_exception_raise_state(
    exception_handler_t exception_port,
    exception_type_t exception,
    mach_exception_data_t code,
    mach_msg_type_number_t code_count,
    thread_state_flavor_t* flavor,
    thread_state_t old_state,
    mach_msg_type_number_t old_state_count,
    thread_state_t new_state,
    mach_msg_type_number_t* new_state_count) {
  NOTREACHED();
  return KERN_FAILURE;
}

kern_return_t catch_mach_exception_raise_state_identity(
    exception_handler_t exception_port,
    thread_t thread,
    task_t task,
    exception_type_t exception,
    mach_exception_data_t code,
    mach_msg_type_number_t code_count,
    thread_state_flavor_t* flavor,
    thread_state_t old_state,
    mach_msg_type_number_t old_state_count,
    thread_state_t new_state,
    mach_msg_type_number_t* new_state_count) {
  NOTREACHED();
  return KERN_FAILURE;
}

}  // extern "C"

namespace {

void PrepareReplyFromRequest(const mach_msg_header_t* in_header,
                             mach_msg_header_t* out_header) {
  out_header->msgh_bits =
      MACH_MSGH_BITS(MACH_MSGH_BITS_REMOTE(in_header->msgh_bits), 0);
  out_header->msgh_remote_port = in_header->msgh_remote_port;
  out_header->msgh_size = sizeof(mig_reply_error_t);
  out_header->msgh_local_port = MACH_PORT_NULL;
  out_header->msgh_id = in_header->msgh_id + 100;
  reinterpret_cast<mig_reply_error_t*>(out_header)->NDR = NDR_record;
}

void SetReplyError(mach_msg_header_t* out_header, kern_return_t error) {
  reinterpret_cast<mig_reply_error_t*>(out_header)->RetCode = error;
}

// There are no predefined constants for these.
enum MachMessageID : mach_msg_id_t {
  kMachMessageIDExceptionRaise = 2401,
  kMachMessageIDExceptionRaiseState = 2402,
  kMachMessageIDExceptionRaiseStateIdentity = 2403,
  kMachMessageIDMachExceptionRaise = 2405,
  kMachMessageIDMachExceptionRaiseState = 2406,
  kMachMessageIDMachExceptionRaiseStateIdentity = 2407,
};

}  // namespace

namespace crashpad {
namespace internal {

ExcServer::ExcServer(ExcServer::Interface* interface)
    : MachMessageServer::Interface(),
      interface_(interface) {
}

bool ExcServer::MachMessageServerFunction(mach_msg_header_t* in_header,
                                          mach_msg_header_t* out_header,
                                          bool* destroy_complex_request) {
  PrepareReplyFromRequest(in_header, out_header);

  switch (in_header->msgh_id) {
    case kMachMessageIDExceptionRaise: {
      // exception_raise(), catch_exception_raise().
      typedef __Request__exception_raise_t Request;
      Request* in_request = reinterpret_cast<Request*>(in_header);
      kern_return_t kr = __MIG_check__Request__exception_raise_t(in_request);
      if (kr != MACH_MSG_SUCCESS) {
        SetReplyError(out_header, kr);
        return true;
      }

      typedef __Reply__exception_raise_t Reply;
      Reply* out_reply = reinterpret_cast<Reply*>(out_header);
      out_reply->RetCode =
          interface_->CatchExceptionRaise(in_header->msgh_local_port,
                                          in_request->thread.name,
                                          in_request->task.name,
                                          in_request->exception,
                                          in_request->code,
                                          in_request->codeCnt,
                                          destroy_complex_request);
      if (out_reply->RetCode != KERN_SUCCESS) {
        return true;
      }

      out_header->msgh_size = sizeof(*out_reply);
      return true;
    }

    case kMachMessageIDExceptionRaiseState: {
      // exception_raise_state(), catch_exception_raise_state().
      typedef __Request__exception_raise_state_t Request;
      Request* in_request = reinterpret_cast<Request*>(in_header);

      // in_request_1 is used for the portion of the request after the codes,
      // which in theory can be variable-length. The check function will set it.
      Request* in_request_1;
      kern_return_t kr = __MIG_check__Request__exception_raise_state_t(
          in_request, &in_request_1);
      if (kr != MACH_MSG_SUCCESS) {
        SetReplyError(out_header, kr);
        return true;
      }

      typedef __Reply__exception_raise_state_t Reply;
      Reply* out_reply = reinterpret_cast<Reply*>(out_header);
      out_reply->new_stateCnt = arraysize(out_reply->new_state);
      out_reply->RetCode =
          interface_->CatchExceptionRaiseState(in_header->msgh_local_port,
                                               in_request->exception,
                                               in_request->code,
                                               in_request->codeCnt,
                                               &in_request_1->flavor,
                                               in_request_1->old_state,
                                               in_request_1->old_stateCnt,
                                               out_reply->new_state,
                                               &out_reply->new_stateCnt);
      if (out_reply->RetCode != KERN_SUCCESS) {
        return true;
      }

      out_reply->flavor = in_request_1->flavor;
      out_header->msgh_size =
          sizeof(*out_reply) - sizeof(out_reply->new_state) +
          sizeof(out_reply->new_state[0]) * out_reply->new_stateCnt;
      return true;
    }

    case kMachMessageIDExceptionRaiseStateIdentity: {
      // exception_raise_state_identity(),
      // catch_exception_raise_state_identity().
      typedef __Request__exception_raise_state_identity_t Request;
      Request* in_request = reinterpret_cast<Request*>(in_header);

      // in_request_1 is used for the portion of the request after the codes,
      // which in theory can be variable-length. The check function will set it.
      Request* in_request_1;
      kern_return_t kr = __MIG_check__Request__exception_raise_state_identity_t(
          in_request, &in_request_1);
      if (kr != MACH_MSG_SUCCESS) {
        SetReplyError(out_header, kr);
        return true;
      }

      typedef __Reply__exception_raise_state_identity_t Reply;
      Reply* out_reply = reinterpret_cast<Reply*>(out_header);
      out_reply->new_stateCnt = arraysize(out_reply->new_state);
      out_reply->RetCode = interface_->CatchExceptionRaiseStateIdentity(
          in_header->msgh_local_port,
          in_request->thread.name,
          in_request->task.name,
          in_request->exception,
          in_request->code,
          in_request->codeCnt,
          &in_request_1->flavor,
          in_request_1->old_state,
          in_request_1->old_stateCnt,
          out_reply->new_state,
          &out_reply->new_stateCnt,
          destroy_complex_request);
      if (out_reply->RetCode != KERN_SUCCESS) {
        return true;
      }

      out_reply->flavor = in_request_1->flavor;
      out_header->msgh_size =
          sizeof(*out_reply) - sizeof(out_reply->new_state) +
          sizeof(out_reply->new_state[0]) * out_reply->new_stateCnt;
      return true;
    }
  }

  SetReplyError(out_header, MIG_BAD_ID);
  return false;
}

mach_msg_size_t ExcServer::MachMessageServerRequestSize() {
  return sizeof(__RequestUnion__exc_subsystem);
}

mach_msg_size_t ExcServer::MachMessageServerReplySize() {
  return sizeof(__ReplyUnion__exc_subsystem);
}

MachExcServer::MachExcServer(MachExcServer::Interface* interface)
    : MachMessageServer::Interface(),
      interface_(interface) {
}

bool MachExcServer::MachMessageServerFunction(mach_msg_header_t* in_header,
                                              mach_msg_header_t* out_header,
                                              bool* destroy_complex_request) {
  PrepareReplyFromRequest(in_header, out_header);

  switch (in_header->msgh_id) {
    case kMachMessageIDMachExceptionRaise: {
      // mach_exception_raise(), catch_mach_exception_raise().
      typedef __Request__mach_exception_raise_t Request;
      Request* in_request = reinterpret_cast<Request*>(in_header);
      kern_return_t kr =
          __MIG_check__Request__mach_exception_raise_t(in_request);
      if (kr != MACH_MSG_SUCCESS) {
        SetReplyError(out_header, kr);
        return true;
      }

      typedef __Reply__mach_exception_raise_t Reply;
      Reply* out_reply = reinterpret_cast<Reply*>(out_header);
      out_reply->RetCode =
          interface_->CatchMachExceptionRaise(in_header->msgh_local_port,
                                              in_request->thread.name,
                                              in_request->task.name,
                                              in_request->exception,
                                              in_request->code,
                                              in_request->codeCnt,
                                              destroy_complex_request);
      if (out_reply->RetCode != KERN_SUCCESS) {
        return true;
      }

      out_header->msgh_size = sizeof(*out_reply);
      return true;
    }

    case kMachMessageIDMachExceptionRaiseState: {
      // mach_exception_raise_state(), catch_mach_exception_raise_state().
      typedef __Request__mach_exception_raise_state_t Request;
      Request* in_request = reinterpret_cast<Request*>(in_header);

      // in_request_1 is used for the portion of the request after the codes,
      // which in theory can be variable-length. The check function will set it.
      Request* in_request_1;
      kern_return_t kr = __MIG_check__Request__mach_exception_raise_state_t(
          in_request, &in_request_1);
      if (kr != MACH_MSG_SUCCESS) {
        SetReplyError(out_header, kr);
        return true;
      }

      typedef __Reply__mach_exception_raise_state_t Reply;
      Reply* out_reply = reinterpret_cast<Reply*>(out_header);
      out_reply->new_stateCnt = arraysize(out_reply->new_state);
      out_reply->RetCode =
          interface_->CatchMachExceptionRaiseState(in_header->msgh_local_port,
                                                   in_request->exception,
                                                   in_request->code,
                                                   in_request->codeCnt,
                                                   &in_request_1->flavor,
                                                   in_request_1->old_state,
                                                   in_request_1->old_stateCnt,
                                                   out_reply->new_state,
                                                   &out_reply->new_stateCnt);
      if (out_reply->RetCode != KERN_SUCCESS) {
        return true;
      }

      out_reply->flavor = in_request_1->flavor;
      out_header->msgh_size =
          sizeof(*out_reply) - sizeof(out_reply->new_state) +
          sizeof(out_reply->new_state[0]) * out_reply->new_stateCnt;
      return true;
    }

    case kMachMessageIDMachExceptionRaiseStateIdentity: {
      // mach_exception_raise_state_identity(),
      // catch_mach_exception_raise_state_identity().
      typedef __Request__mach_exception_raise_state_identity_t Request;
      Request* in_request = reinterpret_cast<Request*>(in_header);

      // in_request_1 is used for the portion of the request after the codes,
      // which in theory can be variable-length. The check function will set it.
      Request* in_request_1;
      kern_return_t kr =
          __MIG_check__Request__mach_exception_raise_state_identity_t(
              in_request, &in_request_1);
      if (kr != MACH_MSG_SUCCESS) {
        SetReplyError(out_header, kr);
        return true;
      }

      typedef __Reply__mach_exception_raise_state_identity_t Reply;
      Reply* out_reply = reinterpret_cast<Reply*>(out_header);
      out_reply->new_stateCnt = arraysize(out_reply->new_state);
      out_reply->RetCode = interface_->CatchMachExceptionRaiseStateIdentity(
          in_header->msgh_local_port,
          in_request->thread.name,
          in_request->task.name,
          in_request->exception,
          in_request->code,
          in_request->codeCnt,
          &in_request_1->flavor,
          in_request_1->old_state,
          in_request_1->old_stateCnt,
          out_reply->new_state,
          &out_reply->new_stateCnt,
          destroy_complex_request);
      if (out_reply->RetCode != KERN_SUCCESS) {
        return true;
      }

      out_reply->flavor = in_request_1->flavor;
      out_header->msgh_size =
          sizeof(*out_reply) - sizeof(out_reply->new_state) +
          sizeof(out_reply->new_state[0]) * out_reply->new_stateCnt;
      return true;
    }
  }

  SetReplyError(out_header, MIG_BAD_ID);
  return false;
}

mach_msg_size_t MachExcServer::MachMessageServerRequestSize() {
  return sizeof(__RequestUnion__mach_exc_subsystem);
}

mach_msg_size_t MachExcServer::MachMessageServerReplySize() {
  return sizeof(__ReplyUnion__mach_exc_subsystem);
}

SimplifiedExcServer::SimplifiedExcServer(
    SimplifiedExcServer::Interface* interface)
    : ExcServer(this),
      ExcServer::Interface(),
      interface_(interface) {
}

kern_return_t SimplifiedExcServer::CatchExceptionRaise(
    exception_handler_t exception_port,
    thread_t thread,
    task_t task,
    exception_type_t exception,
    const exception_data_type_t* code,
    mach_msg_type_number_t code_count,
    bool* destroy_request) {
  thread_state_flavor_t flavor = THREAD_STATE_NONE;
  mach_msg_type_number_t new_state_count = 0;
  return interface_->CatchException(EXCEPTION_DEFAULT,
                                    exception_port,
                                    thread,
                                    task,
                                    exception,
                                    code,
                                    code_count,
                                    &flavor,
                                    NULL,
                                    0,
                                    NULL,
                                    &new_state_count,
                                    destroy_request);
}

kern_return_t SimplifiedExcServer::CatchExceptionRaiseState(
    exception_handler_t exception_port,
    exception_type_t exception,
    const exception_data_type_t* code,
    mach_msg_type_number_t code_count,
    thread_state_flavor_t* flavor,
    const natural_t* old_state,
    mach_msg_type_number_t old_state_count,
    thread_state_t new_state,
    mach_msg_type_number_t* new_state_count) {
  bool destroy_complex_request = false;
  return interface_->CatchException(EXCEPTION_STATE,
                                    exception_port,
                                    MACH_PORT_NULL,
                                    MACH_PORT_NULL,
                                    exception,
                                    code,
                                    code_count,
                                    flavor,
                                    old_state,
                                    old_state_count,
                                    new_state,
                                    new_state_count,
                                    &destroy_complex_request);
}

kern_return_t SimplifiedExcServer::CatchExceptionRaiseStateIdentity(
    exception_handler_t exception_port,
    thread_t thread,
    task_t task,
    exception_type_t exception,
    const exception_data_type_t* code,
    mach_msg_type_number_t code_count,
    thread_state_flavor_t* flavor,
    const natural_t* old_state,
    mach_msg_type_number_t old_state_count,
    thread_state_t new_state,
    mach_msg_type_number_t* new_state_count,
    bool* destroy_request) {
  return interface_->CatchException(EXCEPTION_STATE_IDENTITY,
                                    exception_port,
                                    thread,
                                    task,
                                    exception,
                                    code,
                                    code_count,
                                    flavor,
                                    old_state,
                                    old_state_count,
                                    new_state,
                                    new_state_count,
                                    destroy_request);
}

SimplifiedMachExcServer::SimplifiedMachExcServer(
    SimplifiedMachExcServer::Interface* interface)
    : MachExcServer(this),
      MachExcServer::Interface(),
      interface_(interface) {
}

kern_return_t SimplifiedMachExcServer::CatchMachExceptionRaise(
    exception_handler_t exception_port,
    thread_t thread,
    task_t task,
    exception_type_t exception,
    const mach_exception_data_type_t* code,
    mach_msg_type_number_t code_count,
    bool* destroy_request) {
  thread_state_flavor_t flavor = THREAD_STATE_NONE;
  mach_msg_type_number_t new_state_count = 0;
  return interface_->CatchMachException(
      EXCEPTION_DEFAULT | MACH_EXCEPTION_CODES,
      exception_port,
      thread,
      task,
      exception,
      code,
      code_count,
      &flavor,
      NULL,
      0,
      NULL,
      &new_state_count,
      destroy_request);
}

kern_return_t SimplifiedMachExcServer::CatchMachExceptionRaiseState(
    exception_handler_t exception_port,
    exception_type_t exception,
    const mach_exception_data_type_t* code,
    mach_msg_type_number_t code_count,
    thread_state_flavor_t* flavor,
    const natural_t* old_state,
    mach_msg_type_number_t old_state_count,
    thread_state_t new_state,
    mach_msg_type_number_t* new_state_count) {
  bool destroy_complex_request = false;
  return interface_->CatchMachException(EXCEPTION_STATE | MACH_EXCEPTION_CODES,
                                        exception_port,
                                        MACH_PORT_NULL,
                                        MACH_PORT_NULL,
                                        exception,
                                        code,
                                        code_count,
                                        flavor,
                                        old_state,
                                        old_state_count,
                                        new_state,
                                        new_state_count,
                                        &destroy_complex_request);
}

kern_return_t SimplifiedMachExcServer::CatchMachExceptionRaiseStateIdentity(
    exception_handler_t exception_port,
    thread_t thread,
    task_t task,
    exception_type_t exception,
    const mach_exception_data_type_t* code,
    mach_msg_type_number_t code_count,
    thread_state_flavor_t* flavor,
    const natural_t* old_state,
    mach_msg_type_number_t old_state_count,
    thread_state_t new_state,
    mach_msg_type_number_t* new_state_count,
    bool* destroy_request) {
  return interface_->CatchMachException(
      EXCEPTION_STATE_IDENTITY | MACH_EXCEPTION_CODES,
      exception_port,
      thread,
      task,
      exception,
      code,
      code_count,
      flavor,
      old_state,
      old_state_count,
      new_state,
      new_state_count,
      destroy_request);
}

}  // namespace internal

UniversalMachExcServer::UniversalMachExcServer()
    : MachMessageServer::Interface(),
      internal::SimplifiedExcServer::Interface(),
      internal::SimplifiedMachExcServer::Interface(),
      exc_server_(this),
      mach_exc_server_(this) {
}

bool UniversalMachExcServer::MachMessageServerFunction(
    mach_msg_header_t* in_header,
    mach_msg_header_t* out_header,
    bool* destroy_complex_request) {
  switch (in_header->msgh_id) {
    case kMachMessageIDMachExceptionRaise:
    case kMachMessageIDMachExceptionRaiseState:
    case kMachMessageIDMachExceptionRaiseStateIdentity:
      return mach_exc_server_.MachMessageServerFunction(
          in_header, out_header, destroy_complex_request);
    case kMachMessageIDExceptionRaise:
    case kMachMessageIDExceptionRaiseState:
    case kMachMessageIDExceptionRaiseStateIdentity:
      return exc_server_.MachMessageServerFunction(
          in_header, out_header, destroy_complex_request);
  }

  // Do what the MIG-generated server routines do when they can’t dispatch a
  // message.
  PrepareReplyFromRequest(in_header, out_header);
  SetReplyError(out_header, MIG_BAD_ID);
  return false;
}

mach_msg_size_t UniversalMachExcServer::MachMessageServerRequestSize() {
  return std::max(mach_exc_server_.MachMessageServerRequestSize(),
                  exc_server_.MachMessageServerRequestSize());
}

mach_msg_size_t UniversalMachExcServer::MachMessageServerReplySize() {
  return std::max(mach_exc_server_.MachMessageServerReplySize(),
                  exc_server_.MachMessageServerReplySize());
}

kern_return_t UniversalMachExcServer::CatchException(
    exception_behavior_t behavior,
    exception_handler_t exception_port,
    thread_t thread,
    task_t task,
    exception_type_t exception,
    const exception_data_type_t* code,
    mach_msg_type_number_t code_count,
    thread_state_flavor_t* flavor,
    const natural_t* old_state,
    mach_msg_type_number_t old_state_count,
    thread_state_t new_state,
    mach_msg_type_number_t* new_state_count,
    bool* destroy_complex_request) {
  std::vector<mach_exception_data_type_t> mach_codes;
  mach_codes.reserve(code_count);
  for (size_t index = 0; index < code_count; ++index) {
    mach_codes.push_back(code[index]);
  }

  return CatchMachException(behavior,
                            exception_port,
                            thread,
                            task,
                            exception,
                            code_count ? &mach_codes[0] : NULL,
                            code_count,
                            flavor,
                            old_state,
                            old_state_count,
                            new_state,
                            new_state_count,
                            destroy_complex_request);
}

}  // namespace crashpad
