#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#if !defined(__Fuchsia__)

#include <errno.h>
#include <spawn.h>
#include <sys/wait.h>

#include <vector>

extern "C" char** environ;

#else

#include <launchpad/launchpad.h>
#include <zircon/process.h>
#include <zircon/syscalls.h>

#endif

int main(int argc, char* argv[]) {
  if (argc < 2) {
    errx(EXIT_FAILURE, "usage");  // TODO(): fix the message
  }

  const char* path = argv[1];

#if defined(__Fuchsia__)
  launchpad_t* lp;
  zx_status_t zs = launchpad_create(zx_job_default(), path, &lp);
  if (zs != ZX_OK) {
    errx(EXIT_FAILURE, "launchpad_create: %d", zs);
  }

  zs = launchpad_load_from_file(lp, path);
  if (zs != ZX_OK) {
    errx(EXIT_FAILURE, "launchpad_load_from_file: %d, %s", zs, launchpad_error_message(lp));
  }

  zs = launchpad_set_args(lp, argc - 1, &argv[1]);
  if (zs != ZX_OK) {
    errx(EXIT_FAILURE, "launchpad_set_args: %d, %s", zs, launchpad_error_message(lp));
  }

  launchpad_clone(lp, LP_CLONE_ALL);
  if (zs != ZX_OK) {
    errx(EXIT_FAILURE, "launchpad_clone: %d, %s", zs, launchpad_error_message(lp));
  }

  int stdout_read;
  zs = launchpad_add_pipe(lp, &stdout_read, STDOUT_FILENO);
  if (zs != ZX_OK) {
    errx(EXIT_FAILURE, "launchpad_add_pipe: %d, %s", zs, launchpad_error_message(lp));
  }

  zx_handle_t process;
  const char* error_message;
  zs = launchpad_go(lp, &process, &error_message);
  if (zs != ZX_OK) {
    errx(EXIT_FAILURE, "launchpad_go: %d, %s", zs, error_message);
  }
#else
  std::vector<char*> run_argv;
  for (int i = 1; i < argc; ++i) {
    run_argv.push_back(argv[i]);
  }
  run_argv.push_back(nullptr);

  int pipe_fds[2];
  if (pipe(pipe_fds) != 0) {
    err(EXIT_FAILURE, "pipe");
  }

  posix_spawn_file_actions_t file_actions;
  errno = posix_spawn_file_actions_init(&file_actions);
  if (errno != 0) {
    err(EXIT_FAILURE, "posix_spawn_file_actions_init");
  }
  // TODO(): posix_spawn_file_actions_destroy

  errno = posix_spawn_file_actions_adddup2(&file_actions, pipe_fds[1], STDOUT_FILENO);
  if (errno != 0) {
    err(EXIT_FAILURE, "posix_spawn_file_actions_adddup2");
  }

  pid_t pid;
  errno = posix_spawn(&pid, path, &file_actions, nullptr, &run_argv[0], environ);
  if (errno != 0) {
    err(EXIT_FAILURE, "posix_spawn");
  }

  close(pipe_fds[1]);
  int stdout_read = pipe_fds[0];
#endif

  ssize_t rv;
  do {
    char buf[256];
    rv = read(stdout_read, &buf, sizeof(buf));
    if (rv < 0) {
      err(EXIT_FAILURE, "read");
    }
    printf("FuchsiaRun %ld !! ", rv); fflush(stdout);
    write(STDOUT_FILENO, &buf, rv);
  } while (rv > 0);

#if defined(__Fuchsia__)
  zx_signals_t observed_signals;
  zs = zx_object_wait_one(process, ZX_PROCESS_TERMINATED, ZX_TIME_INFINITE, &observed_signals);
  if (zs != ZX_OK) {
    errx(EXIT_FAILURE, "zx_object_wait_one: %d", zs);
  }
  // check observed_signals & ZX_PROCESS_TERMINATED?

  zx_info_process_t process_info;
  zs = zx_object_get_info(process, ZX_INFO_PROCESS, &process_info, sizeof(process_info), nullptr, nullptr);
  if (zs != ZX_OK) { 
    errx(EXIT_FAILURE, "zx_object_get_info: %d", zs);
  }
  // check process.exited?
  const int status = process_info.return_code;
#else
  int waitpid_status;
  pid_t waitpid_result = waitpid(pid, &waitpid_status, 0);
  if (waitpid_result < 0) {
    err(EXIT_FAILURE, "waitpid");
  }
  // check WIFEXITED?
  const int status = WEXITSTATUS(waitpid_status);

#endif

  return status;
}
