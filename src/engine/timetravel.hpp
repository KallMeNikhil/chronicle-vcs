#pragma once
#include <string>
#include <utility>
#include <vector>

#include "ds/bplustree.hpp"
#include "engine/dag.hpp"
#include "store/repo.hpp"

namespace chr {

using TimeKey = std::pair<int64_t, int32_t>;

Status resolveRevOrTime(Repo& repo, const std::string& spec, Id* commit);
Status commitAtTime(Repo& repo, int64_t t, Id* commit);

void buildTimeIndex(const Dag& dag, BPlusTree<TimeKey, int>* tree);

Status exportTree(Repo& repo, const Id& commit, const std::string& dir, size_t* files);

struct SharingReport {
  size_t commits = 0;
  uint64_t snapshotEntries = 0;
  uint64_t uniqueObjects = 0;
};
Status sharingReport(Repo& repo, size_t lastN, SharingReport* out);

}
