
#include "relaunch_signal.h"

#include <dirent.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#include <array>
#include <chrono>
#include <memory>
#include <vector>

#include "base/logging.h"

const std::string crashedPidArg("--crashed-pid");
const std::string crashedTimeArg("--crashed-time");

bool crashed = false;

long currentTime() {
  auto now = std::chrono::system_clock::now();
  auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
  auto epoch = now_ms.time_since_epoch();
  auto value = std::chrono::duration_cast<std::chrono::milliseconds>(epoch);
  return value.count();
}

std::string exeCmd(const char* cmd) {
  const int kMaxBufferLength = 32;
  std::array<char, kMaxBufferLength> buffer;
  std::string result;
  std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
  if (!pipe)
    return std::string();
  while (!feof(pipe.get())) {
    if (fgets(buffer.data(), kMaxBufferLength, pipe.get()) != nullptr)
      result += buffer.data();
  }
  return result;
}


void killCrashedPidChildren(const std::string& pid) {
  // if crashed process has children kill them
  std::string cmd = "pgrep -P " + pid;
  auto cmdOutput = exeCmd(cmd.c_str());
  if (!cmdOutput.empty()) {
    std::istringstream stream(cmdOutput);
    for (std::string line; std::getline(stream, line);) {
      if (!line.empty()) {
        int pid = std::stoi(line);
        ::kill(static_cast<pid_t>(pid), SIGKILL);
      }
    }
  }
}

std::vector<char*> getRelaunchArgv(const std::string& argvStr,
                                   const std::string& pidCrashed,
                                   bool& maybeCrashLoop) {
  maybeCrashLoop = false;
  std::vector<char*> cargv;
  long crashTime = currentTime();

  std::istringstream stream(argvStr);
  std::string arg, lastArg;
  bool skipNext = false;
  while (std::getline(stream, arg, '|')) {
    if (!arg.empty() && !skipNext) {
      if (arg.compare(crashedPidArg) == 0 || arg.compare(crashedTimeArg) == 0) {
        // don't pass the --crashed-pid or --crashed-time from prev. crashes
        skipNext = true;
      } else {
        // passing c_str() pointers directly to execv() was causing arg
        // value corruption we create a fresh copy here to prevent that
        char* carg = new char[arg.length() + 1];
        strncpy(carg, arg.c_str(), arg.length() + 1);
        cargv.push_back(carg);
      }
    } else {
      skipNext = false;
      if (lastArg.compare(crashedTimeArg) == 0) {
        // current arg is the previous crash time
        // check if the last crash is very recent to prevent crash+relaunch
        // loops
        long lastCrashTime = std::stol(arg);
        long diff = crashTime - lastCrashTime;
        if (diff < 15000) {
          maybeCrashLoop = true;
          LOG(WARNING) << "May be a crash loop! Last Crash @ " << lastCrashTime
                       << " (" << arg << ") Current Crash @ " << crashTime
                       << " Diff=" << diff;
        }
      }
    }
    lastArg = arg;
  }

  // --crashed-pid
  char* carg = new char[crashedPidArg.length() + 1];
  strncpy(carg, crashedPidArg.c_str(), crashedPidArg.length() + 1);
  cargv.push_back(carg);

  // --crashed-pid value
  carg = new char[pidCrashed.length() + 1];
  strncpy(carg, pidCrashed.c_str(), pidCrashed.length() + 1);
  cargv.push_back(carg);

  // --crashed-time
  carg = new char[crashedTimeArg.length() + 1];
  strncpy(carg, crashedTimeArg.c_str(), crashedTimeArg.length() + 1);
  cargv.push_back(carg);

  // --crashed-time value
  std::string crashTimeStr = std::to_string(crashTime);
  carg = new char[crashTimeStr.length() + 1];
  strncpy(carg, crashTimeStr.c_str(), crashTimeStr.length() + 1);
  cargv.push_back(carg);

  cargv.push_back((char*)NULL);

  return cargv;
}

void freeRelaunchArgv(std::vector<char*>& cargv) {
  for (char* const arg : cargv) {
    if (arg) {
      delete[] arg;
    }
  }
}

namespace td {
void SetCrashed() {
  crashed = true;
}

void RelaunchOnCrash(const std::map<std::string, std::string>& annotations) {
  if (crashed) {
    auto appPath = annotations.find("__td-relaunch-path");
    auto pidCrashed = annotations.find("__td-crashed-pid");

    if (appPath != annotations.end() && pidCrashed != annotations.end()) {
      // relaunch
      auto appArgv = annotations.find("__td-relaunch-argv");
      bool hasArgv = appArgv != annotations.end();
      std::string argvStr(hasArgv ? appArgv->second.c_str() : "");
      bool maybeCrashLoop;
      std::vector<char*> cargv =
          getRelaunchArgv(argvStr, pidCrashed->second, maybeCrashLoop);

      LOG(INFO) << "Got __td-relaunch-path and __td-crashed-pid annotations: "
                << appPath->second.c_str() << " (" << pidCrashed->second.c_str()
                << ") maybeCrashLoop=" << maybeCrashLoop << " ARGS=" << argvStr;

      killCrashedPidChildren(pidCrashed->second);

      int returnC = 0;
      if (!maybeCrashLoop) {
        returnC = execvp(appPath->second.c_str(), cargv.data());
      }

      if (returnC == -1) {
        LOG(ERROR) << "execl return code: " << returnC << " error " << errno;
      }

      freeRelaunchArgv(cargv);
    }
  }
}
}  // namespace td
