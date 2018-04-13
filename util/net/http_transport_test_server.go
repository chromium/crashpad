package main

import "bytes"
import "compress/gzip"
import "encoding/binary"
import "fmt"
import "io"
import "net/http"
import "net/http/httptest"
import "os"
import "strconv"
import "strings"

// consumeBodyAndDumpRequest is mostly from from net/http/httputil DumpRequest
// and slightly tweaked because we want the already-de-chunked de-gzip'd version
// than a slightly-more-raw version (mostly for compatibility with the previous
// Python version of this script).

func consumeBodyAndDumpRequest(req *http.Request) ([]byte, error) {
	var b bytes.Buffer

	// By default, print out the unmodified req.RequestURI, which
	// is always set for incoming server requests. But because we
	// previously used req.URL.RequestURI and the docs weren't
	// always so clear about when to use DumpRequest vs
	// DumpRequestOut, fall back to the old way if the caller
	// provides a non-server Request.
	reqURI := req.RequestURI
	if reqURI == "" {
		reqURI = req.URL.RequestURI()
	}

	fmt.Fprintf(&b, "%s %s HTTP/%d.%d\r\n", req.Method, reqURI, req.ProtoMajor, req.ProtoMinor)

	absRequestURI := strings.HasPrefix(req.RequestURI, "http://") || strings.HasPrefix(req.RequestURI, "https://")
	if !absRequestURI {
		host := req.Host
		if host == "" && req.URL != nil {
			host = req.URL.Host
		}
		if host != "" {
			fmt.Fprintf(&b, "Host: %s\r\n", host)
		}
	}

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

// A one-shot testing webserver.
//
// When invoked, this server will write a short integer to stdout, indicating on
// which port the server is listening. It will then read one short integer from
// stdin, indiciating the response code to be sent in response to a request. It
// also reads 16 characters from stdin, which, after having "\r\n" appended,
// will form the response body in a successful response (one with code 200). The
// server will process one HTTP request, deliver the prearranged response to the
// client, and write the entire request to stdout. It will then terminate.

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
			wholeRequest = string(dump[:len(dump)])

			if r.Method == "POST" && r.URL.Path == "/upload" {
				w.WriteHeader(int(responseCode))
				fmt.Fprint(w, string(responseBody[:len(responseBody)]), "\r\n")
				done <- 0
			} else {
				w.WriteHeader(500)
				done <- 1
			}
		}))

	parts := strings.Split(ts.URL, ":")
	port, err := strconv.Atoi(parts[len(parts)-1])
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
