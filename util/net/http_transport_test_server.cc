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

// A one-shot testing webserver.
//
// When invoked, this server will write a short integer to stdout, indicating on
// which port the server is listening. It will then read one integer from stdin,
// indicating the response code to be sent in response to a request. It also
// reads 16 characters from stdin, which, after having "\r\n" appended, will
// form the response body in a successful response (one with code 200). The
// server will process one HTTP request, deliver the prearranged response to the
// client, and write the entire request to stdout. It will then terminate.

#include <stdio.h>

#include <iostream>

// These two headers are missing from httplib.h.
#include <sys/select.h>
#include <sys/time.h>

//#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "../../third_party/cpp-httplib/cpp-httplib/httplib.h"

#include "base/logging.h"
#include "util/file/file_io.h"

std::string dump_headers(const httplib::Headers& headers) {
  std::string s;
  char buf[BUFSIZ];

  for (auto it = headers.begin(); it != headers.end(); ++it) {
    const auto& x = *it;
    snprintf(buf, sizeof(buf), "%s: %s\n", x.first.c_str(), x.second.c_str());
    s += buf;
  }

  return s;
}

void LogRequestAndResponse(const httplib::Request& req,
                           const httplib::Response& res) {
  LOG(ERROR) << req.method << " " << req.path << " " << req.version;
#if 0
  LOG(ERROR) << 

  std::string query;
  for (auto it = req.params.begin(); it != req.params.end(); ++it) {
    const auto& x = *it;
    LOG(ERROR) << "
    snprintf(buf,
             sizeof(buf),
             "%c%s=%s",
             (it == req.params.begin()) ? '?' : '&',
             x.first.c_str(),
             x.second.c_str());
    query += buf;
  }
  snprintf(buf, sizeof(buf), "%s\n", query.c_str());
  s += buf;

  s += dump_headers(req.headers);

  s += "--------------------------------\n";

  snprintf(buf, sizeof(buf), "%d %s\n", res.status, res.version.c_str());
  s += buf;
  s += dump_headers(res.headers);
  s += "\n";

  if (!res.body.empty()) {
    s += res.body;
  }

  s += "\n";

  return s;
#endif
}

int main(void) {
  using namespace crashpad;
  auto version = httplib::HttpVersion::v1_1;

#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
  static constexpr const char kCertFile[] = "cert.pem";
  static constexpr const char kPrivateKeyFile[] = "key.pem";
  httplib::SSLServer svr(kCertFile, kPrivateKeyFile, version);
#else
  httplib::Server svr(version);
#endif

  if (!svr.is_valid()) {
    LOG(ERROR) << "server creation failed";
    return 1;
  }

  svr.post("/upload", [](const auto& req, auto& res) {
    //auto body = dump_headers(req.headers) + dump_multipart_files(req.files);
    res.set_content("hi", "text/plain");
  });

  svr.set_logger([](const auto& req, const auto& res) {
    LogRequestAndResponse(req, res);
  });

  int port = svr.bind_to_any("127.0.0.1");
  fprintf(stderr, "port=%d\n", port);

  CheckedWriteFile(
      StdioFileHandle(StdioStream::kStandardOutput), &port, sizeof(port));

  uint16_t response_code;
  CheckedReadFileExactly(StdioFileHandle(StdioStream::kStandardInput),
                         &response_code,
                         sizeof(response_code));

  uint8_t response[16];
  CheckedReadFileExactly(StdioFileHandle(StdioStream::kStandardInput),
                         &response,
                         sizeof(response));

  svr.listen_after_bind();

  return 0;
}
