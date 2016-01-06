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

#include "util/net/http_transport.h"

#include <windows.h>
#include <stdint.h>
#include <sys/types.h>
#include <winhttp.h>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/scoped_generic.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "package.h"
#include "util/file/file_io.h"
#include "util/net/http_body.h"

namespace crashpad {

namespace {

// PLOG doesn't work for messages from WinHTTP, so we need to use
// FORMAT_MESSAGE_FROM_HMODULE + the dll name manually here.
void LogErrorWinHttpMessage(const char* extra) {
  DWORD error_code = GetLastError();
  char msgbuf[256];
  DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |
                FORMAT_MESSAGE_MAX_WIDTH_MASK | FORMAT_MESSAGE_FROM_HMODULE;
  DWORD len = FormatMessageA(flags,
                             GetModuleHandle(L"winhttp.dll"),
                             error_code,
                             0,
                             msgbuf,
                             arraysize(msgbuf),
                             NULL);
  if (len) {
    LOG(ERROR) << extra << ": " << msgbuf
               << base::StringPrintf(" (0x%X)", error_code);
  } else {
    LOG(ERROR) << base::StringPrintf(
        "Error (0x%X) while retrieving error. (0x%X)",
        GetLastError(),
        error_code);
  }
}

struct ScopedHINTERNETTraits {
  static HINTERNET InvalidValue() {
    return nullptr;
  }
  static void Free(HINTERNET handle) {
    if (handle) {
      if (!WinHttpCloseHandle(handle)) {
        LogErrorWinHttpMessage("WinHttpCloseHandle");
      }
    }
  }
};

using ScopedHINTERNET = base::ScopedGeneric<HINTERNET, ScopedHINTERNETTraits>;

class HTTPTransportWin final : public HTTPTransport {
 public:
  HTTPTransportWin();
  ~HTTPTransportWin() override;

  bool ExecuteSynchronously(std::string* response_body) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(HTTPTransportWin);
};

HTTPTransportWin::HTTPTransportWin() : HTTPTransport() {
}

HTTPTransportWin::~HTTPTransportWin() {
}

bool HTTPTransportWin::ExecuteSynchronously(std::string* response_body) {
  ScopedHINTERNET session(
      WinHttpOpen(base::UTF8ToUTF16(PACKAGE_NAME "/" PACKAGE_VERSION).c_str(),
                  WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                  WINHTTP_NO_PROXY_NAME,
                  WINHTTP_NO_PROXY_BYPASS,
                  0));
  if (!session.get()) {
    LogErrorWinHttpMessage("WinHttpOpen");
    return false;
  }

  int timeout_in_ms = static_cast<int>(timeout() * 1000);
  if (!WinHttpSetTimeouts(session.get(),
                          timeout_in_ms,
                          timeout_in_ms,
                          timeout_in_ms,
                          timeout_in_ms)) {
    LogErrorWinHttpMessage("WinHttpSetTimeouts");
    return false;
  }

  URL_COMPONENTS url_components = {0};
  url_components.dwStructSize = sizeof(URL_COMPONENTS);
  url_components.dwHostNameLength = 1;
  url_components.dwUrlPathLength = 1;
  url_components.dwExtraInfoLength = 1;
  std::wstring url_wide(base::UTF8ToUTF16(url()));
  // dwFlags = ICU_REJECT_USERPWD fails on XP. See "Community Additions" at:
  // https://msdn.microsoft.com/en-us/library/aa384092.aspx
  if (!WinHttpCrackUrl(
          url_wide.c_str(), 0, 0, &url_components)) {
    LogErrorWinHttpMessage("WinHttpCrackUrl");
    return false;
  }
  DCHECK(url_components.nScheme == INTERNET_SCHEME_HTTP ||
         url_components.nScheme == INTERNET_SCHEME_HTTPS);
  std::wstring host_name(url_components.lpszHostName,
                         url_components.dwHostNameLength);
  std::wstring url_path(url_components.lpszUrlPath,
                        url_components.dwUrlPathLength);
  std::wstring extra_info(url_components.lpszExtraInfo,
                          url_components.dwExtraInfoLength);

  ScopedHINTERNET connect(WinHttpConnect(
      session.get(), host_name.c_str(), url_components.nPort, 0));
  if (!connect.get()) {
    LogErrorWinHttpMessage("WinHttpConnect");
    return false;
  }

  ScopedHINTERNET request(WinHttpOpenRequest(
      connect.get(),
      base::UTF8ToUTF16(method()).c_str(),
      url_path.c_str(),
      nullptr,
      WINHTTP_NO_REFERER,
      WINHTTP_DEFAULT_ACCEPT_TYPES,
      url_components.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE
                                                      : 0));
  if (!request.get()) {
    LogErrorWinHttpMessage("WinHttpOpenRequest");
    return false;
  }

  // Add headers to the request.
  for (const auto& pair : headers()) {
    std::wstring header_string =
        base::UTF8ToUTF16(pair.first) + L": " + base::UTF8ToUTF16(pair.second);
    if (!WinHttpAddRequestHeaders(
            request.get(),
            header_string.c_str(),
            base::checked_cast<DWORD>(header_string.size()),
            WINHTTP_ADDREQ_FLAG_ADD)) {
      LogErrorWinHttpMessage("WinHttpAddRequestHeaders");
      return false;
    }
  }

  // We need the Content-Length up front, so buffer in memory. We should modify
  // the interface to not require this, and then use WinHttpWriteData after
  // WinHttpSendRequest.
  std::vector<uint8_t> post_data;

  // Write the body of a POST if any.
  const size_t kBufferSize = 4096;
  for (;;) {
    uint8_t buffer[kBufferSize];
    FileOperationResult bytes_to_write =
        body_stream()->GetBytesBuffer(buffer, sizeof(buffer));
    if (bytes_to_write == 0)
      break;
    post_data.insert(post_data.end(), buffer, buffer + bytes_to_write);
  }

  if (!WinHttpSendRequest(request.get(),
                          WINHTTP_NO_ADDITIONAL_HEADERS,
                          0,
                          &post_data[0],
                          base::checked_cast<DWORD>(post_data.size()),
                          base::checked_cast<DWORD>(post_data.size()),
                          0)) {
    LogErrorWinHttpMessage("WinHttpSendRequest");
    return false;
  }

  if (!WinHttpReceiveResponse(request.get(), nullptr)) {
    LogErrorWinHttpMessage("WinHttpReceiveResponse");
    return false;
  }

  DWORD status_code = 0;
  DWORD sizeof_status_code = sizeof(status_code);

  if (!WinHttpQueryHeaders(
          request.get(),
          WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
          WINHTTP_HEADER_NAME_BY_INDEX,
          &status_code,
          &sizeof_status_code,
          WINHTTP_NO_HEADER_INDEX)) {
    LogErrorWinHttpMessage("WinHttpQueryHeaders");
    return false;
  }

  if (status_code != 200) {
    LOG(ERROR) << base::StringPrintf("HTTP status %d", status_code);
    return false;
  }

  if (response_body) {
    response_body->clear();

    // There isn’t any reason to call WinHttpQueryDataAvailable(), because it
    // returns the number of bytes available to be read without blocking at the
    // time of the call, not the number of bytes until end-of-file. This method,
    // which executes synchronously, is only concerned with reading until EOF.
    DWORD bytes_read = 0;
    do {
      char read_buffer[kBufferSize];
      if (!WinHttpReadData(
              request.get(), read_buffer, sizeof(read_buffer), &bytes_read)) {
        LogErrorWinHttpMessage("WinHttpReadData");
        return false;
      }

      response_body->append(read_buffer, bytes_read);
    } while (bytes_read > 0);
  }

  return true;
}

}  // namespace

// static
scoped_ptr<HTTPTransport> HTTPTransport::Create() {
  return scoped_ptr<HTTPTransportWin>(new HTTPTransportWin);
}

}  // namespace crashpad
