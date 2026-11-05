#pragma once
#include <string>

#include "core/status.hpp"

namespace chr {

class RepoLock {
 public:
  RepoLock() = default;
  ~RepoLock() { release(); }
  RepoLock(const RepoLock&) = delete;
  RepoLock& operator=(const RepoLock&) = delete;

  Status acquire(const std::string& chronicleDir, const std::string& name = "lock");
  void release();

 private:
  std::string path_;
};

}
