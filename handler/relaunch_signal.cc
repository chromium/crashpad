
#include "relaunch_signal.h"

bool crashed = false;

namespace crashpad {
void SetCrashed() {
  crashed = true;
}

bool HasCrashed() {
  return crashed;
}
}  // namespace crashpad
