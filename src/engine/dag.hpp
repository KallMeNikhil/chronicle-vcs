#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "store/repo.hpp"

namespace chr {

struct CommitNode {
  Id id;
  int64_t time = 0;
  std::vector<int> parents;
  std::vector<int> children;
  int32_t generation = 0;
};

class Dag {
 public:
  Status load(ObjectStore& store, const std::vector<Id>& tips);
  Status loadGraph(const std::vector<Id>& ids, const std::vector<int64_t>& times,
                   const std::vector<std::vector<int>>& parents);
  int idxOf(const Id& id) const;
  const CommitNode& node(int i) const { return nodes_[static_cast<size_t>(i)]; }
  int size() const { return static_cast<int>(nodes_.size()); }

  bool isAncestor(int a, int b) const;
  std::vector<char> reach(int x) const;
  std::vector<int> mergeBases(int a, int b) const;
  int chooseBase(const std::vector<int>& bases) const;

 private:
  std::vector<CommitNode> nodes_;
  std::unordered_map<Id, int, IdHash> index_;
};

Status refTips(Repo& repo, std::vector<Id>* out);

}
