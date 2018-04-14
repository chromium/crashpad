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

#include <stdio.h>

#include <iostream>

// These two headers are missing from httplib.h.
#include <sys/select.h>
#include <sys/time.h>

#include "../../third_party/cpp-httplib/cpp-httplib/httplib.h"

#define SERVER_CERT_FILE "./cert.pem"
#define SERVER_PRIVATE_KEY_FILE "./key.pem"

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

std::string log(const httplib::Request& req, const httplib::Response& res) {
  std::string s;
  char buf[BUFSIZ];

  s += "================================\n";

  snprintf(buf,
           sizeof(buf),
           "%s %s %s",
           req.method.c_str(),
           req.path.c_str(),
           req.version.c_str());
  s += buf;

  std::string query;
  for (auto it = req.params.begin(); it != req.params.end(); ++it) {
    const auto& x = *it;
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
}

int main(void) {
  auto version = httplib::HttpVersion::v1_1;

#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
  httplib::SSLServer svr(SERVER_CERT_FILE, SERVER_PRIVATE_KEY_FILE, version);
#else
  httplib::Server svr(version);
#endif

  if (!svr.is_valid()) {
    printf("server has an error...\n");
    return -1;
  }

  svr.get("/",
          [=](const auto& /*req*/, auto& res) { res.set_redirect("/hi"); });

  svr.get("/hi", [](const auto& /*req*/, auto& res) {
    res.set_content("Hello World!\n", "text/plain");
  });

  svr.get("/dump", [](const auto& req, auto& res) {
    res.set_content(dump_headers(req.headers), "text/plain");
  });

  svr.get("/stop", [&](const auto& /*req*/, auto& /*res*/) { svr.stop(); });

  svr.set_error_handler([](const auto& /*req*/, auto& res) {
    const char* fmt = "<p>Error Status: <span style='color:red;'>%d</span></p>";
    char buf[BUFSIZ];
    snprintf(buf, sizeof(buf), fmt, res.status);
    res.set_content(buf, "text/html");
  });

  svr.set_logger([](const auto& req, const auto& res) {
    printf("%s", log(req, res).c_str());
  });

  svr.listen("127.0.0.1", 0);

  return 0;
}

// vim: et ts=4 sw=4 cin cino={1s ff=unix
