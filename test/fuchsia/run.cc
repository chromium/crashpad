#include <err.h>
#include <launchpad/launchpad.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zircon/process.h>
#include <zircon/syscalls.h>
#include <zircon/syscalls/log.h>

#include <algorithm>
#include <string>
#include <vector>

namespace {

void AppendEscapedCharacter(std::string& s, const char c) {
  switch (c) {
    case '\a':
      s.append("\\a");
      break;
    case '\b':
      s.append("\\b");
      break;
    case '\f':
      s.append("\\f");
      break;
    case '\n':
      s.append("\\n");
      break;
    case '\r':
      s.append("\\r");
      break;
    case '\t':
      s.append("\\t");
      break;
    case '\v':
      s.append("\\v");
      break;
    case '\\':
      s.append("\\\\");
      break;

    default:
      s.append("\\x");
      static constexpr char kHexDigits[] = "0123456789abcdef";
      s.append(1, kHexDigits[static_cast<unsigned char>(c) >> 4]);
      s.append(1, kHexDigits[c & 0xf]);
      break;
  }
}

void AppendCharacter(std::string& s, const char c) {
  if ((c < ' ' && c != '\n') || c == '\\' || (c == ' ' && s.empty())) {
    return AppendEscapedCharacter(s, c);
  }
  s.append(1, c);
}

void WriteOutput(int id, std::string& s) {
  if (s.empty()) {
    return;
  }

  if (s.back() != '\n') {
    s.append("\\\n");
  } else if (s.size() >= 2 && s[s.size() - 2] == ' ') {
    s.resize(s.size() - 2);
    AppendEscapedCharacter(s, ' ');
    s.append(1, '\n');
  }
  FILE* stream = (id == STDOUT_FILENO) ? stdout : stderr;
  fprintf(stream, "%d ", id);
  if (fwrite(s.data(), 1, s.size(), stream) != s.size()) {
    err(EXIT_FAILURE, "fwrite");
  }
  s.clear();
}

bool HandleStreamInput(int in_fd, int id) {
  while (true) {
    char in_buf[4096];
    ssize_t in_size = read(in_fd, &in_buf, sizeof(in_buf));
    if (in_size < 0) {
      err(EXIT_FAILURE, "read");
    }
    if (in_size == 0) {
      return false;
    }

    std::string out_string;
    for (size_t i = 0; i < static_cast<size_t>(in_size); ++i) {
      const char c = in_buf[i];
      AppendCharacter(out_string, c);

      // This is the maximum amount of data that can be carried in a log
      // message, minus 4. 4 is chosen because that’s the largest number of
      // bytes that the next input character could cause to be appended to
      // out_string. This uses >= instead of > because if the next character
      // uses a 4-byte hex escape, it will not end in a newline and will need
      // room for a trailing backslash too.
      //
      // Minus another two for <id><space>.
      constexpr size_t kMaxOutputLine =
          ZX_LOG_RECORD_MAX - sizeof(zx_log_record_t) - 6;
      if (c == '\n' || out_string.size() >= kMaxOutputLine) {
        WriteOutput(id, out_string);
      }
    }

    WriteOutput(id, out_string);

    if (static_cast<size_t>(in_size) < sizeof(in_buf)) {
      return true;
    }
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc < 2) {
    errx(EXIT_FAILURE, "usage");  // TODO(): fix the message
  }

  setenv("GTEST_COLOR", "yes", 1);

  const char* path = argv[1];

  launchpad_t* lp;
  zx_status_t zs = launchpad_create(zx_job_default(), path, &lp);
  if (zs != ZX_OK) {
    errx(EXIT_FAILURE, "launchpad_create: %d", zs);
  }

  zs = launchpad_load_from_file(lp, path);
  if (zs != ZX_OK) {
    errx(EXIT_FAILURE,
         "launchpad_load_from_file: %d, %s",
         zs,
         launchpad_error_message(lp));
  }

  zs = launchpad_set_args(lp, argc - 1, &argv[1]);
  if (zs != ZX_OK) {
    errx(EXIT_FAILURE,
         "launchpad_set_args: %d, %s",
         zs,
         launchpad_error_message(lp));
  }

  launchpad_clone(lp, LP_CLONE_ALL);
  if (zs != ZX_OK) {
    errx(EXIT_FAILURE,
         "launchpad_clone: %d, %s",
         zs,
         launchpad_error_message(lp));
  }

  int stdout_read;
  zs = launchpad_add_pipe(lp, &stdout_read, STDOUT_FILENO);
  if (zs != ZX_OK) {
    errx(EXIT_FAILURE,
         "launchpad_add_pipe: %d, %s",
         zs,
         launchpad_error_message(lp));
  }

  int stderr_read;
  zs = launchpad_add_pipe(lp, &stderr_read, STDERR_FILENO);
  if (zs != ZX_OK) {
    errx(EXIT_FAILURE,
         "launchpad_add_pipe: %d, %s",
         zs,
         launchpad_error_message(lp));
  }

  zx_handle_t process;
  const char* error_message;
  zs = launchpad_go(lp, &process, &error_message);
  if (zs != ZX_OK) {
    errx(EXIT_FAILURE, "launchpad_go: %d, %s", zs, error_message);
  }

  while (stdout_read != -1 || stderr_read != -1) {
    std::vector<pollfd> poll_fds;
    int sei = 0;
    if (stdout_read != -1) {
      poll_fds.emplace_back(
          pollfd{.fd = stdout_read, .events = POLLIN | POLLHUP});
      sei = 1;
    }
    if (stderr_read != -1) {
      poll_fds.emplace_back(
          pollfd{.fd = stderr_read, .events = POLLIN | POLLHUP});
    }
    int poll_rv = poll(poll_fds.data(), poll_fds.size(), -1);
    if (poll_rv < 0) {
      err(EXIT_FAILURE, "select");
    }
    if (stdout_read != -1 && poll_fds[0].revents) {
      if (!HandleStreamInput(stdout_read, STDOUT_FILENO)) {
        close(stdout_read);
        stdout_read = -1;
      }
    }
    if (stderr_read != -1 && poll_fds[sei].revents) {
      if (!HandleStreamInput(stderr_read, STDERR_FILENO)) {
        close(stderr_read);
        stderr_read = -1;
      }
    }
  }

  zx_signals_t observed_signals;
  zs = zx_object_wait_one(
      process, ZX_PROCESS_TERMINATED, ZX_TIME_INFINITE, &observed_signals);
  if (zs != ZX_OK) {
    errx(EXIT_FAILURE, "zx_object_wait_one: %d", zs);
  }
  // check observed_signals & ZX_PROCESS_TERMINATED?

  zx_info_process_t process_info;
  zs = zx_object_get_info(process,
                          ZX_INFO_PROCESS,
                          &process_info,
                          sizeof(process_info),
                          nullptr,
                          nullptr);
  if (zs != ZX_OK) {
    errx(EXIT_FAILURE, "zx_object_get_info: %d", zs);
  }
  // check process.exited?
  const int status = process_info.return_code;

  return status;
}
