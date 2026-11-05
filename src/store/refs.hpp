#pragma once
#include <string>
#include <utility>
#include <vector>

#include "core/id.hpp"
#include "core/status.hpp"

namespace chr {

struct HeadInfo {
  bool detached = false;
  std::string branch;
  bool unborn = false;
  Id id;
};

class Refs {
 public:
  void setDir(const std::string& chronicleDir) { dir_ = chronicleDir; }

  Status readHead(HeadInfo* out) const;
  Status setHeadBranch(const std::string& branch) const;
  Status setHeadDetached(const Id& id) const;
  Status advanceHead(const Id& id) const;

  bool branchExists(const std::string& name) const;
  Status readBranch(const std::string& name, Id* out) const;
  Status writeBranch(const std::string& name, const Id& id) const;
  Status deleteBranch(const std::string& name) const;
  Status listBranches(std::vector<std::pair<std::string, Id>>* out) const;

  static bool validBranchName(const std::string& name);

 private:
  std::string branchPath(const std::string& name) const { return dir_ + "/refs/heads/" + name; }
  Status walk(const std::string& rel, std::vector<std::pair<std::string, Id>>* out) const;
  std::string dir_;
};

}
