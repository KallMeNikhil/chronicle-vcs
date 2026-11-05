#include "store/lock.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <string>

namespace chr {

Status RepoLock::acquire(const std::string& chronicleDir, const std::string& name) {
  std::string p = chronicleDir + "/" + name;
  int fd = open(p.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644);
  if (fd < 0)
    return RepoErr("repository is locked (stale lock? remove .chronicle/" + name + ")");
  std::string pid = std::to_string(getpid()) + "\n";
  ssize_t ignored = write(fd, pid.data(), pid.size());
  (void)ignored;
  close(fd);
  path_ = p;
  return Status::Ok();
}

void RepoLock::release() {
  if (!path_.empty()) {
    unlink(path_.c_str());
    path_.clear();
  }
}

}
