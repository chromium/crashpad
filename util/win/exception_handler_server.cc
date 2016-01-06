// Copyright 2015 The Crashpad Authors. All rights reserved.
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

#include "util/win/exception_handler_server.h"

#include <sddl.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#include <utility>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "minidump/minidump_file_writer.h"
#include "snapshot/crashpad_info_client_options.h"
#include "snapshot/win/process_snapshot_win.h"
#include "util/file/file_writer.h"
#include "util/misc/random_string.h"
#include "util/misc/tri_state.h"
#include "util/misc/uuid.h"
#include "util/win/get_function.h"
#include "util/win/handle.h"
#include "util/win/registration_protocol_win.h"
#include "util/win/scoped_local_alloc.h"
#include "util/win/xp_compat.h"

namespace crashpad {

namespace {

// We create two pipe instances, so that there's one listening while the
// PipeServiceProc is processing a registration.
const size_t kPipeInstances = 2;

// Wraps CreateNamedPipe() to create a single named pipe instance.
//
// If first_instance is true, the named pipe instance will be created with
// FILE_FLAG_FIRST_PIPE_INSTANCE. This ensures that the the pipe name is not
// already in use when created. The first instance will be created with an
// untrusted integrity SACL so instances of this pipe can be connected to by
// processes of any integrity level.
HANDLE CreateNamedPipeInstance(const std::wstring& pipe_name,
                               bool first_instance) {
  SECURITY_ATTRIBUTES security_attributes;
  SECURITY_ATTRIBUTES* security_attributes_pointer = nullptr;
  ScopedLocalAlloc scoped_sec_desc;

  if (first_instance) {
    // Pre-Vista does not have integrity levels.
    const DWORD version = GetVersion();
    const DWORD major_version = LOBYTE(LOWORD(version));
    const bool is_vista_or_later = major_version >= 6;
    if (is_vista_or_later) {
      // Mandatory Label, no ACE flags, no ObjectType, integrity level
      // untrusted.
      const wchar_t kSddl[] = L"S:(ML;;;;;S-1-16-0)";

      PSECURITY_DESCRIPTOR sec_desc;
      PCHECK(ConvertStringSecurityDescriptorToSecurityDescriptor(
          kSddl, SDDL_REVISION_1, &sec_desc, nullptr))
          << "ConvertStringSecurityDescriptorToSecurityDescriptor";

      // Take ownership of the allocated SECURITY_DESCRIPTOR.
      scoped_sec_desc.reset(sec_desc);

      memset(&security_attributes, 0, sizeof(security_attributes));
      security_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
      security_attributes.lpSecurityDescriptor = sec_desc;
      security_attributes.bInheritHandle = FALSE;
      security_attributes_pointer = &security_attributes;
    }
  }

  return CreateNamedPipe(
      pipe_name.c_str(),
      PIPE_ACCESS_DUPLEX | (first_instance ? FILE_FLAG_FIRST_PIPE_INSTANCE : 0),
      PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
      kPipeInstances,
      512,
      512,
      0,
      security_attributes_pointer);
}

decltype(GetNamedPipeClientProcessId)* GetNamedPipeClientProcessIdFunction() {
  static const auto get_named_pipe_client_process_id =
      GET_FUNCTION(L"kernel32.dll", ::GetNamedPipeClientProcessId);
  return get_named_pipe_client_process_id;
}

HANDLE DuplicateEvent(HANDLE process, HANDLE event) {
  HANDLE handle;
  if (DuplicateHandle(GetCurrentProcess(),
                      event,
                      process,
                      &handle,
                      SYNCHRONIZE | EVENT_MODIFY_STATE,
                      false,
                      0)) {
    return handle;
  }
  return nullptr;
}

}  // namespace

namespace internal {

//! \brief Context information for the named pipe handler threads.
class PipeServiceContext {
 public:
  PipeServiceContext(HANDLE port,
                     HANDLE pipe,
                     ExceptionHandlerServer::Delegate* delegate,
                     base::Lock* clients_lock,
                     std::set<internal::ClientData*>* clients,
                     uint64_t shutdown_token)
      : port_(port),
        pipe_(pipe),
        delegate_(delegate),
        clients_lock_(clients_lock),
        clients_(clients),
        shutdown_token_(shutdown_token) {}

  HANDLE port() const { return port_; }
  HANDLE pipe() const { return pipe_.get(); }
  ExceptionHandlerServer::Delegate* delegate() const { return delegate_; }
  base::Lock* clients_lock() const { return clients_lock_; }
  std::set<internal::ClientData*>* clients() const { return clients_; }
  uint64_t shutdown_token() const { return shutdown_token_; }

 private:
  HANDLE port_;  // weak
  ScopedKernelHANDLE pipe_;
  ExceptionHandlerServer::Delegate* delegate_;  // weak
  base::Lock* clients_lock_;  // weak
  std::set<internal::ClientData*>* clients_;  // weak
  uint64_t shutdown_token_;

  DISALLOW_COPY_AND_ASSIGN(PipeServiceContext);
};

//! \brief The context data for registered threadpool waits.
//!
//! This object must be created and destroyed on the main thread. Access must be
//! guarded by use of the lock() with the exception of the threadpool wait
//! variables which are accessed only by the main thread.
class ClientData {
 public:
  ClientData(HANDLE port,
             ExceptionHandlerServer::Delegate* delegate,
             ScopedKernelHANDLE process,
             WinVMAddress crash_exception_information_address,
             WinVMAddress non_crash_exception_information_address,
             WinVMAddress debug_critical_section_address,
             WAITORTIMERCALLBACK crash_dump_request_callback,
             WAITORTIMERCALLBACK non_crash_dump_request_callback,
             WAITORTIMERCALLBACK process_end_callback)
      : crash_dump_request_thread_pool_wait_(INVALID_HANDLE_VALUE),
        non_crash_dump_request_thread_pool_wait_(INVALID_HANDLE_VALUE),
        process_end_thread_pool_wait_(INVALID_HANDLE_VALUE),
        lock_(),
        port_(port),
        delegate_(delegate),
        crash_dump_requested_event_(
            CreateEvent(nullptr, false /* auto reset */, false, nullptr)),
        non_crash_dump_requested_event_(
            CreateEvent(nullptr, false /* auto reset */, false, nullptr)),
        non_crash_dump_completed_event_(
            CreateEvent(nullptr, false /* auto reset */, false, nullptr)),
        process_(std::move(process)),
        crash_exception_information_address_(
            crash_exception_information_address),
        non_crash_exception_information_address_(
            non_crash_exception_information_address),
        debug_critical_section_address_(debug_critical_section_address) {
    RegisterThreadPoolWaits(crash_dump_request_callback,
                            non_crash_dump_request_callback,
                            process_end_callback);
  }

  ~ClientData() {
    // It is important that this only access the threadpool waits (it's called
    // from the main thread) until the waits are unregistered, to ensure that
    // any outstanding callbacks are complete.
    UnregisterThreadPoolWaits();
  }

  base::Lock* lock() { return &lock_; }
  HANDLE port() const { return port_; }
  ExceptionHandlerServer::Delegate* delegate() const { return delegate_; }
  HANDLE crash_dump_requested_event() const {
    return crash_dump_requested_event_.get();
  }
  HANDLE non_crash_dump_requested_event() const {
    return non_crash_dump_requested_event_.get();
  }
  HANDLE non_crash_dump_completed_event() const {
    return non_crash_dump_completed_event_.get();
  }
  WinVMAddress crash_exception_information_address() const {
    return crash_exception_information_address_;
  }
  WinVMAddress non_crash_exception_information_address() const {
    return non_crash_exception_information_address_;
  }
  WinVMAddress debug_critical_section_address() const {
    return debug_critical_section_address_;
  }
  HANDLE process() const { return process_.get(); }

 private:
  void RegisterThreadPoolWaits(
      WAITORTIMERCALLBACK crash_dump_request_callback,
      WAITORTIMERCALLBACK non_crash_dump_request_callback,
      WAITORTIMERCALLBACK process_end_callback) {
    if (!RegisterWaitForSingleObject(&crash_dump_request_thread_pool_wait_,
                                     crash_dump_requested_event_.get(),
                                     crash_dump_request_callback,
                                     this,
                                     INFINITE,
                                     WT_EXECUTEDEFAULT)) {
      LOG(ERROR) << "RegisterWaitForSingleObject crash dump requested";
    }

    if (!RegisterWaitForSingleObject(&non_crash_dump_request_thread_pool_wait_,
                                     non_crash_dump_requested_event_.get(),
                                     non_crash_dump_request_callback,
                                     this,
                                     INFINITE,
                                     WT_EXECUTEDEFAULT)) {
      LOG(ERROR) << "RegisterWaitForSingleObject non-crash dump requested";
    }

    if (!RegisterWaitForSingleObject(&process_end_thread_pool_wait_,
                                     process_.get(),
                                     process_end_callback,
                                     this,
                                     INFINITE,
                                     WT_EXECUTEONLYONCE)) {
      LOG(ERROR) << "RegisterWaitForSingleObject process end";
    }
  }

  // This blocks until outstanding calls complete so that we know it's safe to
  // delete this object. Because of this, it must be executed on the main
  // thread, not a threadpool thread.
  void UnregisterThreadPoolWaits() {
    UnregisterWaitEx(crash_dump_request_thread_pool_wait_,
                     INVALID_HANDLE_VALUE);
    crash_dump_request_thread_pool_wait_ = INVALID_HANDLE_VALUE;
    UnregisterWaitEx(non_crash_dump_request_thread_pool_wait_,
                     INVALID_HANDLE_VALUE);
    non_crash_dump_request_thread_pool_wait_ = INVALID_HANDLE_VALUE;
    UnregisterWaitEx(process_end_thread_pool_wait_, INVALID_HANDLE_VALUE);
    process_end_thread_pool_wait_ = INVALID_HANDLE_VALUE;
  }

  // These are only accessed on the main thread.
  HANDLE crash_dump_request_thread_pool_wait_;
  HANDLE non_crash_dump_request_thread_pool_wait_;
  HANDLE process_end_thread_pool_wait_;

  base::Lock lock_;
  // Access to these fields must be guarded by lock_.
  HANDLE port_;  // weak
  ExceptionHandlerServer::Delegate* delegate_;  // weak
  ScopedKernelHANDLE crash_dump_requested_event_;
  ScopedKernelHANDLE non_crash_dump_requested_event_;
  ScopedKernelHANDLE non_crash_dump_completed_event_;
  ScopedKernelHANDLE process_;
  WinVMAddress crash_exception_information_address_;
  WinVMAddress non_crash_exception_information_address_;
  WinVMAddress debug_critical_section_address_;

  DISALLOW_COPY_AND_ASSIGN(ClientData);
};

}  // namespace internal

ExceptionHandlerServer::Delegate::~Delegate() {
}

ExceptionHandlerServer::ExceptionHandlerServer(bool persistent)
    : pipe_name_(),
      port_(CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1)),
      first_pipe_instance_(),
      clients_lock_(),
      clients_(),
      persistent_(persistent) {
}

ExceptionHandlerServer::~ExceptionHandlerServer() {
}

void ExceptionHandlerServer::SetPipeName(const std::wstring& pipe_name) {
  DCHECK(pipe_name_.empty());
  DCHECK(!pipe_name.empty());

  pipe_name_ = pipe_name;
}

std::wstring ExceptionHandlerServer::CreatePipe() {
  DCHECK(!first_pipe_instance_.is_valid());

  int tries = 5;
  std::string pipe_name_base =
      base::StringPrintf("\\\\.\\pipe\\crashpad_%d_", GetCurrentProcessId());
  std::wstring pipe_name;
  do {
    pipe_name = base::UTF8ToUTF16(pipe_name_base + RandomString());

    first_pipe_instance_.reset(CreateNamedPipeInstance(pipe_name, true));

    // CreateNamedPipe() is documented as setting the error to
    // ERROR_ACCESS_DENIED if FILE_FLAG_FIRST_PIPE_INSTANCE is specified and the
    // pipe name is already in use. However it may set the error to other codes
    // such as ERROR_PIPE_BUSY (if the pipe already exists and has reached its
    // maximum instance count) or ERROR_INVALID_PARAMETER (if the pipe already
    // exists and its attributes differ from those specified to
    // CreateNamedPipe()). Some of these errors may be ambiguous: for example,
    // ERROR_INVALID_PARAMETER may also occur if CreateNamedPipe() is called
    // incorrectly even in the absence of an existing pipe by the same name.
    //
    // Rather than chasing down all of the possible errors that might indicate
    // that a pipe name is already in use, retry up to a few times on any error.
  } while (!first_pipe_instance_.is_valid() && --tries);

  PCHECK(first_pipe_instance_.is_valid()) << "CreateNamedPipe";

  SetPipeName(pipe_name);
  return pipe_name;
}

void ExceptionHandlerServer::Run(Delegate* delegate) {
  uint64_t shutdown_token = base::RandUint64();
  ScopedKernelHANDLE thread_handles[kPipeInstances];
  for (size_t i = 0; i < arraysize(thread_handles); ++i) {
    HANDLE pipe;
    if (first_pipe_instance_.is_valid()) {
      pipe = first_pipe_instance_.release();
    } else {
      pipe = CreateNamedPipeInstance(pipe_name_, i == 0);
      PCHECK(pipe != INVALID_HANDLE_VALUE) << "CreateNamedPipe";
    }

    // Ownership of this object (and the pipe instance) is given to the new
    // thread. We close the thread handles at the end of the scope. They clean
    // up the context object and the pipe instance on termination.
    internal::PipeServiceContext* context =
        new internal::PipeServiceContext(port_.get(),
                                         pipe,
                                         delegate,
                                         &clients_lock_,
                                         &clients_,
                                         shutdown_token);
    thread_handles[i].reset(
        CreateThread(nullptr, 0, &PipeServiceProc, context, 0, nullptr));
    PCHECK(thread_handles[i].is_valid()) << "CreateThread";
  }

  delegate->ExceptionHandlerServerStarted();

  // This is the main loop of the server. Most work is done on the threadpool,
  // other than process end handling which is posted back to this main thread,
  // as we must unregister the threadpool waits here.
  for (;;) {
    OVERLAPPED* ov = nullptr;
    ULONG_PTR key = 0;
    DWORD bytes = 0;
    GetQueuedCompletionStatus(port_.get(), &bytes, &key, &ov, INFINITE);
    if (!key) {
      // Shutting down.
      break;
    }

    // Otherwise, this is a request to unregister and destroy the given client.
    // delete'ing the ClientData blocks in UnregisterWaitEx to ensure all
    // outstanding threadpool waits are complete. This is important because the
    // process handle can be signalled *before* the dump request is signalled.
    internal::ClientData* client = reinterpret_cast<internal::ClientData*>(key);
    base::AutoLock lock(clients_lock_);
    clients_.erase(client);
    delete client;
    if (!persistent_ && clients_.empty())
      break;
  }

  // Signal to the named pipe instances that they should terminate.
  for (size_t i = 0; i < arraysize(thread_handles); ++i) {
    ClientToServerMessage message;
    memset(&message, 0, sizeof(message));
    message.type = ClientToServerMessage::kShutdown;
    message.shutdown.token = shutdown_token;
    ServerToClientMessage response;
    SendToCrashHandlerServer(pipe_name_,
                             reinterpret_cast<ClientToServerMessage&>(message),
                             &response);
  }

  for (auto& handle : thread_handles)
    WaitForSingleObject(handle.get(), INFINITE);

  // Deleting ClientData does a blocking wait until the threadpool executions
  // have terminated when unregistering them.
  {
    base::AutoLock lock(clients_lock_);
    for (auto* client : clients_)
      delete client;
    clients_.clear();
  }
}

void ExceptionHandlerServer::Stop() {
  // Post a null key (third argument) to trigger shutdown.
  PostQueuedCompletionStatus(port_.get(), 0, 0, nullptr);
}

// This function must be called with service_context.pipe() already connected to
// a client pipe. It exchanges data with the client and adds a ClientData record
// to service_context->clients().
//
// static
bool ExceptionHandlerServer::ServiceClientConnection(
    const internal::PipeServiceContext& service_context) {
  ClientToServerMessage message;

  if (!LoggingReadFile(service_context.pipe(), &message, sizeof(message)))
    return false;

  switch (message.type) {
    case ClientToServerMessage::kShutdown: {
      if (message.shutdown.token != service_context.shutdown_token()) {
        LOG(ERROR) << "forged shutdown request, got: "
                   << message.shutdown.token;
        return false;
      }
      ServerToClientMessage shutdown_response = {};
      LoggingWriteFile(service_context.pipe(),
                       &shutdown_response,
                       sizeof(shutdown_response));
      return true;
    }

    case ClientToServerMessage::kRegister:
      // Handled below.
      break;

    default:
      LOG(ERROR) << "unhandled message type: " << message.type;
      return false;
  }

  if (message.registration.version != RegistrationRequest::kMessageVersion) {
    LOG(ERROR) << "unexpected version. got: " << message.registration.version
               << " expecting: " << RegistrationRequest::kMessageVersion;
    return false;
  }

  decltype(GetNamedPipeClientProcessId)* get_named_pipe_client_process_id =
      GetNamedPipeClientProcessIdFunction();
  if (get_named_pipe_client_process_id) {
    // GetNamedPipeClientProcessId is only available on Vista+.
    DWORD real_pid = 0;
    if (get_named_pipe_client_process_id(service_context.pipe(), &real_pid) &&
        message.registration.client_process_id != real_pid) {
      LOG(ERROR) << "forged client pid, real pid: " << real_pid
                 << ", got: " << message.registration.client_process_id;
      return false;
    }
  }

  // We attempt to open the process as us. This is the main case that should
  // almost always succeed as the server will generally be more privileged. If
  // we're running as a different user, it may be that we will fail to open
  // the process, but the client will be able to, so we make a second attempt
  // having impersonated the client.
  HANDLE client_process = OpenProcess(
      kXPProcessAllAccess, false, message.registration.client_process_id);
  if (!client_process) {
    if (!ImpersonateNamedPipeClient(service_context.pipe())) {
      PLOG(ERROR) << "ImpersonateNamedPipeClient";
      return false;
    }
    client_process = OpenProcess(
        kXPProcessAllAccess, false, message.registration.client_process_id);
    PCHECK(RevertToSelf());
    if (!client_process) {
      LOG(ERROR) << "failed to open " << message.registration.client_process_id;
      return false;
    }
  }

  internal::ClientData* client;
  {
    base::AutoLock lock(*service_context.clients_lock());
    client = new internal::ClientData(
        service_context.port(),
        service_context.delegate(),
        ScopedKernelHANDLE(client_process),
        message.registration.crash_exception_information,
        message.registration.non_crash_exception_information,
        message.registration.critical_section_address,
        &OnCrashDumpEvent,
        &OnNonCrashDumpEvent,
        &OnProcessEnd);
    service_context.clients()->insert(client);
  }

  // Duplicate the events back to the client so they can request a dump.
  ServerToClientMessage response;
  response.registration.request_crash_dump_event =
      HandleToInt(DuplicateEvent(
          client->process(), client->crash_dump_requested_event()));
  response.registration.request_non_crash_dump_event =
      HandleToInt(DuplicateEvent(
          client->process(), client->non_crash_dump_requested_event()));
  response.registration.non_crash_dump_completed_event =
      HandleToInt(DuplicateEvent(
          client->process(), client->non_crash_dump_completed_event()));

  if (!LoggingWriteFile(service_context.pipe(), &response, sizeof(response)))
    return false;

  return false;
}

// static
DWORD __stdcall ExceptionHandlerServer::PipeServiceProc(void* ctx) {
  internal::PipeServiceContext* service_context =
      reinterpret_cast<internal::PipeServiceContext*>(ctx);
  DCHECK(service_context);

  for (;;) {
    bool ret = !!ConnectNamedPipe(service_context->pipe(), nullptr);
    if (!ret && GetLastError() != ERROR_PIPE_CONNECTED) {
      PLOG(ERROR) << "ConnectNamedPipe";
    } else if (ServiceClientConnection(*service_context)) {
      break;
    }
    DisconnectNamedPipe(service_context->pipe());
  }

  delete service_context;

  return 0;
}

// static
void __stdcall ExceptionHandlerServer::OnCrashDumpEvent(void* ctx, BOOLEAN) {
  // This function is executed on the thread pool.
  internal::ClientData* client = reinterpret_cast<internal::ClientData*>(ctx);
  base::AutoLock lock(*client->lock());

  // Capture the exception.
  unsigned int exit_code = client->delegate()->ExceptionHandlerServerException(
      client->process(),
      client->crash_exception_information_address(),
      client->debug_critical_section_address());

  TerminateProcess(client->process(), exit_code);
}

// static
void __stdcall ExceptionHandlerServer::OnNonCrashDumpEvent(void* ctx, BOOLEAN) {
  // This function is executed on the thread pool.
  internal::ClientData* client = reinterpret_cast<internal::ClientData*>(ctx);
  base::AutoLock lock(*client->lock());

  // Capture the exception.
  client->delegate()->ExceptionHandlerServerException(
      client->process(),
      client->non_crash_exception_information_address(),
      client->debug_critical_section_address());

  bool result = !!SetEvent(client->non_crash_dump_completed_event());
  PLOG_IF(ERROR, !result) << "SetEvent";
}

// static
void __stdcall ExceptionHandlerServer::OnProcessEnd(void* ctx, BOOLEAN) {
  // This function is executed on the thread pool.
  internal::ClientData* client = reinterpret_cast<internal::ClientData*>(ctx);
  base::AutoLock lock(*client->lock());

  // Post back to the main thread to have it delete this client record.
  PostQueuedCompletionStatus(client->port(), 0, ULONG_PTR(client), nullptr);
}

}  // namespace crashpad
