#pragma once
#include <string>
#include <vector>

#include "store/repo.hpp"

namespace chr {

struct BranchReport {
  std::string name;
  bool here = false, there = false;
  Id local, remote;
  size_t ahead = 0, behind = 0;
  bool haveBase = false;
  Id base;
};

Status compareRepos(Repo& local, Repo& other, std::vector<BranchReport>* out);
Status fetchObjects(Repo& local, Repo& other, const Id& tip, size_t* copied);

}
