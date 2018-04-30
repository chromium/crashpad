// Copyright 2017 The Crashpad Authors. All rights reserved.
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

#include "handler/fuchsia/exception_handler_server.h"

#include <zircon/syscalls/port.h>
#include <zircon/syscalls/exception.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "util/fuchsia/exception_port_key.h"

namespace crashpad {

namespace {

const char* ExceptionTypeName(uint32_t type) {
  switch (type) {
    case ZX_EXCP_GENERAL:
      return "ZX_EXCP_GENERAL";
    case ZX_EXCP_FATAL_PAGE_FAULT:
      return "ZX_EXCP_FATAL_PAGE_FAULT";
    case ZX_EXCP_UNDEFINED_INSTRUCTION:
      return "ZX_EXCP_UNDEFINED_INSTRUCTION";
    case ZX_EXCP_SW_BREAKPOINT:
      return "ZX_EXCP_SW_BREAKPOINT";
    case ZX_EXCP_HW_BREAKPOINT:
      return "ZX_EXCP_HW_BREAKPOINT";
    case ZX_EXCP_UNALIGNED_ACCESS:
      return "ZX_EXCP_UNALIGNED_ACCESS";
    case ZX_EXCP_THREAD_STARTING:
      return "ZX_EXCP_THREAD_STARTING";
    case ZX_EXCP_THREAD_EXITING:
      return "ZX_EXCP_THREAD_EXITING";
    case ZX_EXCP_POLICY_ERROR:
      return "ZX_EXCP_POLICY_ERROR";
    default:
      return "<unknown>";
  }
}

}  // namespace

ExceptionHandlerServer::ExceptionHandlerServer(zx_handle_t exception_port)
    : exception_port_(exception_port) {}

ExceptionHandlerServer::~ExceptionHandlerServer() {}

void ExceptionHandlerServer::Run(CrashReportExceptionHandler* handler) {
  while (true) {
    zx_port_packet_t packet;
    LOG(ERROR) << "going to wait for a exception packet";
    zx_status_t status =
        zx_port_wait(exception_port_.get(), ZX_TIME_INFINITE, &packet, 1);
    if (status != ZX_OK) {
      ZX_LOG(ERROR, status) << "zx_port_wait, aborting";
      return;
    }

    if (packet.key != kExceptionPortKey) {
      LOG(ERROR) << "unexpected packet key, ignoring";
      continue;
    }

    LOG(ERROR) << "GOT EXCEPTION: pid=" << packet.exception.pid
               << ", tid=" << packet.exception.tid
               << ", type=" << ExceptionTypeName(packet.type) << " ("
               << packet.type << ")";
  }
}

}  // namespace crashpad
