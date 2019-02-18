#include <dlfcn.h>

int main(int argc, char* argv[]) {
  using MainType = int (*)(int, char*[]);

  void* handle = dlopen(argv[1], RTLD_LAZY | RTLD_GLOBAL);
  if (handle) {
    MainType crashpad_main = reinterpret_cast<MainType>(dlsym(handle, "crashpad_handler_main"));
    if (crashpad_main) {
      return crashpad_main(argc - 1, argv + 1);
    }
  }
  return 1;
}
