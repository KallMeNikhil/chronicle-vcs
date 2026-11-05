#pragma once
#include <string>
#include <vector>

#include "core/id.hpp"
#include "core/status.hpp"

namespace chr {

struct MergeState {
  bool active = false;
  std::string kind = "merge";
  Id theirs;
  std::string branch;
  std::string message;
  std::vector<std::string> conflicts;

  Status load(const std::string& chronicleDir);
  Status save(const std::string& chronicleDir) const;
  static Status remove(const std::string& chronicleDir);
  bool markResolved(const std::string& path);
};

}
