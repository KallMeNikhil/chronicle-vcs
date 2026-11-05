#pragma once
#include <string>
#include <utility>
#include <vector>

#include "store/repo.hpp"

namespace chr {

struct FsckReport {
  size_t checked = 0;
  std::vector<std::string> corrupt;
  std::vector<std::pair<Id, Id>> missing;
  std::vector<std::pair<ObjType, Id>> dangling;
  bool ok() const { return corrupt.empty() && missing.empty(); }
};

Status runFsck(Repo& repo, FsckReport* out);

}
