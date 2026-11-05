#pragma once
#include <string>
#include <vector>

#include "engine/worktree.hpp"
#include "store/objstore.hpp"

namespace chr {

struct TreeChange {
  char kind;
  std::string path;
  FlatEntry oldE;
  FlatEntry newE;
};

Status diffTrees(ObjectStore& store, const Id* a, const Id* b, std::vector<TreeChange>* out,
                 const std::string& prefix = "");
std::vector<TreeChange> diffFlat(const FlatTree& a, const FlatTree& b);

Status lookupPath(ObjectStore& store, const Id& root, const std::string& path, TreeEntry* out, bool* found);

}
