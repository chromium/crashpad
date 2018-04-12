package main

import "encoding/binary"
import "fmt"
import "net/http"
import "net/http/httptest"
import "net/http/httputil"
import "os"
import "strconv"
import "strings"

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
			dump, err := httputil.DumpRequest(r, true)
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
