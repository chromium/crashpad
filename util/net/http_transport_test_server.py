#!/usr/bin/env python
# coding: utf-8

# Copyright 2014 The Crashpad Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""A one-shot testing webserver.

When invoked, this server will write a short integer to stdout, indiciating on
which port the server is listening. It will then read one integer from stdin,
indiciating the response code to be sent in response to a request. It also reads
16 characters from stdin, which, after having "\r\n" appended, will form the
response body in a successful response (one with code 200). The server will
process one HTTP request, deliver the prearranged response to the client, and
write the entire request to stdout. It will then terminate.

This server is written in Python since it provides a simple HTTP stack, and
because parsing Chunked encoding is safer and easier in a memory-safe language.
This could easily have been written in C++ instead.
"""

import BaseHTTPServer
import struct
import sys

class BufferedReadFile(object):
  """A File-like object that stores all read contents into a buffer."""

  def __init__(self, real_file):
    self.file = real_file
    self.buffer = ""

  def read(self, size=-1):
    buf = self.file.read(size)
    self.buffer += buf
    return buf

  def readline(self, size=-1):
    buf = self.file.readline(size)
    self.buffer += buf
    return buf

  def flush(self):
    self.file.flush()

  def close(self):
    self.file.close()


class RequestHandler(BaseHTTPServer.BaseHTTPRequestHandler):
  # Everything to be written to stdout is collected into this string. It can’t
  # be written to stdout until after the HTTP transaction is complete, because
  # stdout is a pipe being read by a test program that’s also the HTTP client.
  # The test program expects to complete the entire HTTP transaction before it
  # even starts reading this script’s stdout. If the stdout pipe buffer fills up
  # during an HTTP transaction, deadlock would result.
  raw_request = ''

  response_code = 500
  response_body = ''

  def handle_one_request(self):
    # Wrap the rfile in the buffering file object so that the raw header block
    # can be written to stdout after it is parsed.
    self.rfile = BufferedReadFile(self.rfile)
    BaseHTTPServer.BaseHTTPRequestHandler.handle_one_request(self)

  def do_POST(self):
    RequestHandler.raw_request = self.rfile.buffer
    self.rfile.buffer = ''

    if self.headers.get('Transfer-Encoding', '') == 'Chunked':
      body = self.handle_chunked_encoding()
    else:
      length = int(self.headers.get('Content-Length', -1))
      body = self.rfile.read(length)

    RequestHandler.raw_request += body

    self.send_response(self.response_code)
    self.end_headers()
    if self.response_code == 200:
      self.wfile.write(self.response_body)
      self.wfile.write('\r\n')

  def handle_chunked_encoding(self):
    """This parses a "Transfer-Encoding: Chunked" body in accordance with
    RFC 7230 §4.1. This returns the result as a string.
    """
    body = ''
    chunk_size = self.read_chunk_size()
    while chunk_size > 0:
      # Read the body.
      data = self.rfile.read(chunk_size)
      chunk_size -= len(data)
      body += data

      # Finished reading this chunk.
      if chunk_size == 0:
        # Read through any trailer fields.
        trailer_line = self.rfile.readline()
        while trailer_line.strip() != '':
          trailer_line = self.rfile.readline()

        # Read the chunk size.
        chunk_size = self.read_chunk_size()
    return body

  def read_chunk_size(self):
    # Read the whole line, including the \r\n.
    chunk_size_and_ext_line = self.rfile.readline()
    # Look for a chunk extension.
    chunk_size_end = chunk_size_and_ext_line.find(';')
    if chunk_size_end == -1:
      # No chunk extensions; just encounter the end of line.
      chunk_size_end = chunk_size_and_ext_line.find('\r')
    if chunk_size_end == -1:
      self.send_response(400)  # Bad request.
      return -1
    return int(chunk_size_and_ext_line[:chunk_size_end], base=16)


def Main():
  if sys.platform == 'win32':
    import os, msvcrt
    msvcrt.setmode(sys.stdout.fileno(), os.O_BINARY)

  # Start the server.
  server = BaseHTTPServer.HTTPServer(('127.0.0.1', 0), RequestHandler)

  # Write the port as an unsigned short to the parent process.
  sys.stdout.write(struct.pack('=H', server.server_address[1]))
  sys.stdout.flush()

  # Read the desired test response code as an unsigned short and the desired
  # response body as a 16-byte string from the parent process.
  RequestHandler.response_code, RequestHandler.response_body = \
      struct.unpack('=H16s', sys.stdin.read(struct.calcsize('=H16s')))

  # Handle the request.
  server.handle_request()

  # Share the entire request with the test program, which will validate it.
  sys.stdout.write(RequestHandler.raw_request)
  sys.stdout.flush()

if __name__ == '__main__':
  Main()
