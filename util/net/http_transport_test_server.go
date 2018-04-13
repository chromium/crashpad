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

package main

import (
	"bytes"
	"compress/gzip"
	"encoding/binary"
	"fmt"
	"io"
	"net/http"
	"net/http/httptest"
	"net/url"
	"os"
	"strconv"
	"strings"
)

// http_transport_test_server is one-shot testing webserver.
//
// When invoked, this server will write a short integer to stdout, indicating on
// which port the server is listening. It will then read one short integer from
// stdin, indiciating the response code to be sent in response to a request. It
// also reads 16 characters from stdin, which, after having "\r\n" appended,
// will form the response body in a successful response (one with code 200). The
// server will process one HTTP request, deliver the prearranged response to the
// client, and write the entire request to stdout. It will then terminate.

// consumeBodyAndDumpRequest is mostly from from net/http/httputil DumpRequest
// and slightly tweaked because we want the already-de-chunked de-gzip'd version
// than a slightly-more-raw version (mostly for compatibility with the previous
// Python version of this script).
func consumeBodyAndDumpRequest(req *http.Request) ([]byte, error) {
	var b bytes.Buffer

	fmt.Fprintf(&b, "%s %s HTTP/%d.%d\r\n", req.Method, req.RequestURI, req.ProtoMajor, req.ProtoMinor)
	fmt.Fprintf(&b, "Host: %s\r\n", req.Host)

	if len(req.TransferEncoding) > 0 {
		fmt.Fprintf(&b, "Transfer-Encoding: %s\r\n", strings.Join(req.TransferEncoding, ","))
	}
	if req.Close {
		fmt.Fprintf(&b, "Connection: close\r\n")
	}

	var reqWriteExcludeHeaderDump = map[string]bool{
		"Host":              true, // not in Header map anyway
		"Transfer-Encoding": true,
		"Trailer":           true,
	}

	err := req.Header.WriteSubset(&b, reqWriteExcludeHeaderDump)
	if err != nil {
		return nil, err
	}

	io.WriteString(&b, "\r\n")

	if req.Body != nil {
		if req.Header.Get("Content-Encoding") == "gzip" {
			req.Body, err = gzip.NewReader(req.Body)
		}
		var dest io.Writer = &b
		_, err = io.Copy(dest, req.Body)
	}

	if err != nil {
		return nil, err
	}
	return b.Bytes(), nil
}

func main() {
	var responseCode uint16
	var responseBody [16]byte
	var wholeRequest string
	done := make(chan int)
	ts := httptest.NewServer(
		http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			dump, err := consumeBodyAndDumpRequest(r)
			if err != nil {
				panic(err)
			}
			wholeRequest = string(dump[:])

			if r.Method == "POST" && r.URL.Path == "/upload" {
				w.WriteHeader(int(responseCode))
				fmt.Fprint(w, string(responseBody[:]), "\r\n")
				done <- 0
			} else {
				w.WriteHeader(500)
				done <- 1
			}
		}))

	asUrl, err := url.Parse(ts.URL)
	if err != nil {
		panic(err)
	}
	port, err := strconv.Atoi(asUrl.Port())
	if err != nil {
		panic(err)
	}
	binary.Write(os.Stdout, binary.LittleEndian, uint16(port))
	err = binary.Read(os.Stdin, binary.LittleEndian, &responseCode)
	if err != nil {
		panic(err)
	}
	binary.Read(os.Stdin, binary.LittleEndian, &responseBody)

	rc := <-done

	fmt.Fprint(os.Stdout, wholeRequest)

	ts.Close()
	os.Exit(rc)
}
