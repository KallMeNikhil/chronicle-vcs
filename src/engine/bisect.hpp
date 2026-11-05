#pragma once
#include <string>
#include <vector>

#include "engine/dag.hpp"
#include "store/repo.hpp"

namespace chr {

struct BisectState {
  bool active = false;
  bool haveBad = false;
  Id bad;
  std::vector<Id> goods, skips;
  std::string orig;
  std::string strategy = "linear";
  bool haveNext = false;
  Id next;
  std::vector<std::string> log;

  Status load(const std::string& chronicleDir);
  Status save(const std::string& chronicleDir) const;
};

struct BisectStep {
  bool done = false;
  bool onlySkipped = false;
  Id firstBad;
  Id test;
  size_t remaining = 0;
  std::vector<Id> left;
  std::string warning;
};

Status bisectCandidates(const Dag& dag, const BisectState& st, std::vector<int>* out);
Status bisectNext(ObjectStore& store, const BisectState& st, BisectStep* out);
int pickLinear(const std::vector<char>& skipped);

}
