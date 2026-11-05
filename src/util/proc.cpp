#include "util/proc.hpp"

#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>

namespace chr {

Status runProcess(const std::string& dir, const std::vector<std::string>& argv, ProcResult* out) {
  if (argv.empty()) return UsageErr("no command given");
  std::fflush(stdout);
  std::fflush(stderr);
  std::vector<char*> args;
  for (const std::string& a : argv) args.push_back(const_cast<char*>(a.c_str()));
  args.push_back(nullptr);
  pid_t pid = fork();
  if (pid < 0) return Err(std::string("fork failed: ") + std::strerror(errno));
  if (pid == 0) {
    if (chdir(dir.c_str()) != 0) _exit(127);
    execvp(args[0], args.data());
    std::fprintf(stderr, "chronicle: cannot run '%s': %s\n", args[0], std::strerror(errno));
    _exit(127);
  }
  int status = 0;
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) return Err(std::string("waitpid failed: ") + std::strerror(errno));
  }
  out->exited = WIFEXITED(status);
  out->code = out->exited ? WEXITSTATUS(status) : (WIFSIGNALED(status) ? WTERMSIG(status) : 0);
  return Status::Ok();
}

}
