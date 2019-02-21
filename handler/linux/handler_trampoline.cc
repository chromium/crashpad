#include <dlfcn.h>

// The first argument passed to the trampoline is the name of the native library
// exporting the symbol `crashpad_handler_main`. The remaining arguments are the
// same as for `HandlerMain()`.
int main(int argc, char* argv[]) {
  using MainType = int (*)(int, char*[]);
  void* handle = dlopen(argv[1], RTLD_LAZY | RTLD_GLOBAL);
  if (!handle) {
    return 1;
  }

  MainType crashpad_main =
      reinterpret_cast<MainType>(dlsym(handle, "crashpad_handler_main"));
  if (!crashpad_main) {
    return 1;
  }

  return crashpad_main(argc, argv);
}
