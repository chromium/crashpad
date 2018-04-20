#if 1
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

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "tools/tool_support.h"
#include "util/file/file_io.h"

#if COMPILER_MSVC
#pragma warning(push)
#pragma warning(disable: 4244 4245 4267 4702)
#endif

#define CPPHTTPLIB_ZLIB_SUPPORT
#include "third_party/cpp-httplib/cpp-httplib/httplib.h"

#if COMPILER_MSVC
#pragma warning(pop)
#endif

namespace crashpad {
namespace {

int HttpTransportTestServerMain(int argc, char* argv[]) {
  httplib::Server svr(httplib::HttpVersion::v1_0);

  if (!svr.is_valid()) {
    LOG(ERROR) << "server creation failed";
    return 1;
  }

  uint16_t response_code;
  char response[16];

  std::string to_stdout;

  svr.post("/upload",
           [&response, &response_code, &svr, &to_stdout](
               const httplib::Request& req, httplib::Response& res) {
             res.status = response_code;
             if (response_code == 200) {
               res.set_content(std::string(response, 16) + "\r\n",
                               "text/plain");
             } else {
               res.set_content("error", "text/plain");
             }

             for (const auto& h : req.headers) {
               fprintf(stderr,
                       "HEADER IN SERVER DUMP: '%s' => '%s'\n",
                       h.first.c_str(),
                       h.second.c_str());
               to_stdout += base::StringPrintf(
                   "%s: %s\r\n", h.first.c_str(), h.second.c_str());
             }
             to_stdout += "\r\n";
             to_stdout += req.body;

             svr.stop();
           });

  int port = svr.bind_to_any_port("127.0.0.1");

  CheckedWriteFile(
      StdioFileHandle(StdioStream::kStandardOutput), &port, sizeof(port));

  CheckedReadFileExactly(StdioFileHandle(StdioStream::kStandardInput),
                         &response_code,
                         sizeof(response_code));

  CheckedReadFileExactly(StdioFileHandle(StdioStream::kStandardInput),
                         &response,
                         sizeof(response));

  svr.listen_after_bind();

  fprintf(stderr, "WHOLE REQUEST DUMP FROM SERVER = '%s'\n", to_stdout.c_str());
  usleep(10000000);
  LoggingWriteFile(StdioFileHandle(StdioStream::kStandardOutput),
                   to_stdout.data(),
                   to_stdout.size());

  return 0;
}

} // namespace
} // namespace crashpad

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
int main(int argc, char* argv[]) {
  return crashpad::HttpTransportTestServerMain(argc, argv);
}
#elif defined(OS_WIN)
int wmain(int argc, wchar_t* argv[]) {
  return crashpad::ToolSupport::Wmain(
      argc, argv, crashpad::HttpTransportTestServerMain);
}
#endif  // OS_POSIX

#else

//
//  sample.cc
//
//  Copyright (c) 2012 Yuji Hirose. All rights reserved.
//  The Boost Software License 1.0
//

#define CPPHTTPLIB_ZLIB_SUPPORT
#include "third_party/cpp-httplib/cpp-httplib/httplib.h"
#include <cstdio>
#include <chrono>

#define SERVER_CERT_FILE "./cert.pem"
#define SERVER_PRIVATE_KEY_FILE "./key.pem"

using namespace httplib;

std::string dump_headers(const Headers& headers)
{
    std::string s;
    char buf[BUFSIZ];

    for (auto it = headers.begin(); it != headers.end(); ++it) {
       const auto& x = *it;
       snprintf(buf, sizeof(buf), "%s: %s\n", x.first.c_str(), x.second.c_str());
       s += buf;
    }

    return s;
}

std::string log(const Request& req, const Response& res)
{
    std::string s;
    char buf[BUFSIZ];

    s += "================================\n";

    snprintf(buf, sizeof(buf), "%s %s %s", req.method.c_str(), req.path.c_str(), req.version.c_str());
    s += buf;

    std::string query;
    for (auto it = req.params.begin(); it != req.params.end(); ++it) {
       const auto& x = *it;
       snprintf(buf, sizeof(buf), "%c%s=%s",
           (it == req.params.begin()) ? '?' : '&', x.first.c_str(), x.second.c_str());
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
}

int main(void)
{
    auto version = httplib::HttpVersion::v1_0;

#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    SSLServer svr(SERVER_CERT_FILE, SERVER_PRIVATE_KEY_FILE, version);
#else
    Server svr(version);
#endif

    if (!svr.is_valid()) {
        printf("server has an error...\n");
        return -1;
    }

    svr.get("/", [=](const auto& /*req*/, auto& res) {
        res.set_redirect("/hi");
    });

    svr.get("/hi", [](const auto& /*req*/, auto& res) {
        res.set_content("Hello World!\n", "text/plain");
    });

    svr.get("/slow", [](const auto& /*req*/, auto& res) {
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(2s);
        res.set_content("Slow...\n", "text/plain");
    });

    svr.get("/dump", [](const auto& req, auto& res) {
        res.set_content(dump_headers(req.headers), "text/plain");
    });

    svr.get("/stop", [&](const auto& /*req*/, auto& /*res*/) {
        svr.stop();
    });

    svr.set_error_handler([](const auto& /*req*/, auto& res) {
        const char* fmt = "<p>Error Status: <span style='color:red;'>%d</span></p>";
        char buf[BUFSIZ];
        snprintf(buf, sizeof(buf), fmt, res.status);
        res.set_content(buf, "text/html");
    });

    svr.set_logger([](const auto& req, const auto& res) {
        printf("%s", log(req, res).c_str());
    });

    int port = svr.bind_to_any_port("127.0.0.1");
    printf("on port: %d\n", port);
    svr.listen_after_bind();

    return 0;
}
// vim: et ts=4 sw=4 cin cino={1s ff=unix

#endif
