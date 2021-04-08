#ifndef CRASHPAD_HANDLER_RELAUNCH_SIGNAL_H_
#define CRASHPAD_HANDLER_RELAUNCH_SIGNAL_H_

#include <string>
#include <map>

namespace td {
  void SetCrashed();

  void RelaunchOnCrash(const std::map<std::string, std::string>& annotations);
}

#endif  // CRASHPAD_HANDLER_RELAUNCH_SIGNAL_H_
