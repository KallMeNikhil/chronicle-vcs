#pragma once
#include <string>
#include <vector>

#include "core/status.hpp"

namespace chr {

class Ignore {
 public:
  Status load(const std::string& worktreeRoot);
  void addPattern(const std::string& line);
  bool ignored(const std::string& rel, bool isDir) const;

 private:
  struct Rule {
    std::string glob;
    bool dirOnly;
  };
  bool matchOne(const std::string& rel, bool isDir) const;
  std::vector<Rule> rules_;
};

}
