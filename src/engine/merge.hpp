#pragma once
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "engine/worktree.hpp"

namespace chr {

struct Diff3Result {
  std::vector<std::string> lines;
  int conflicts = 0;
};

Diff3Result diff3(const std::vector<std::string>& base, const std::vector<std::string>& ours,
                  const std::vector<std::string>& theirs, const std::string& oursLabel,
                  const std::string& theirsLabel);

struct MergeOutcome {
  FlatTree index;
  std::map<std::string, std::string> markerFiles;
  std::vector<std::pair<std::string, std::string>> conflicts;
  bool clean() const { return conflicts.empty(); }
};

Status mergeTrees(ObjectStore& store, const Id* base, const Id* ours, const Id* theirs,
                  const std::string& oursLabel, const std::string& theirsLabel, MergeOutcome* out);

Status applyMergeOutcome(Repo& repo, Index& index, const FlatTree& ours, const MergeOutcome& m);

}
