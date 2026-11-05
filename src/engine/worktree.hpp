#pragma once
#include <map>
#include <string>
#include <vector>

#include "store/ignore.hpp"
#include "store/index.hpp"
#include "store/repo.hpp"
#include "util/fs.hpp"

namespace chr {

struct FlatEntry {
  uint32_t mode = kModeFile;
  Id id;
  bool operator==(const FlatEntry& o) const { return mode == o.mode && id == o.id; }
  bool operator!=(const FlatEntry& o) const { return !(*this == o); }
};
using FlatTree = std::map<std::string, FlatEntry>;

Status flattenTree(ObjectStore& store, const Id& tree, FlatTree* out, const std::string& prefix = "");
Status commitTreeId(ObjectStore& store, const Id& commit, Id* tree);
Status flattenCommit(ObjectStore& store, const Id& commit, FlatTree* out);
Status headFlat(Repo& repo, FlatTree* out, HeadInfo* head = nullptr);
FlatTree indexFlat(const Index& index);

Status walkWorktree(const Repo& repo, const Ignore& ignore, const std::string& startRel,
                    std::map<std::string, fsu::FileStat>* out, std::vector<std::string>* errors);

Status hashWorkFile(Repo& repo, const std::string& rel, const fsu::FileStat& st, bool store,
                    IndexEntry* out);
bool statClean(const IndexEntry& e, const fsu::FileStat& st, int64_t indexWriteNs);
Status workFileDiffers(Repo& repo, const std::string& rel, const IndexEntry& e, int64_t indexWriteNs,
                       bool* differs);

struct StatusReport {
  std::vector<std::pair<char, std::string>> staged;
  std::vector<std::pair<char, std::string>> unstaged;
  std::vector<std::string> untracked;
  bool clean() const { return staged.empty() && unstaged.empty(); }
};
Status computeStatus(Repo& repo, const Index& index, StatusReport* out, bool wantUntracked = true);

Status writeWorkFile(Repo& repo, const std::string& rel, const FlatEntry& e, IndexEntry* stamped);
Status removeWorkFile(Repo& repo, const std::string& rel);

Status switchTree(Repo& repo, Index& index, const FlatTree& from, const FlatTree& to);
Status checkUntrackedOverwrite(Repo& repo, const Index& index, const FlatTree& from, const FlatTree& to);
Status applyTree(Repo& repo, Index& index, const FlatTree& from, const FlatTree& to);
Status hardResetTree(Repo& repo, Index& index, const FlatTree& tracked, const FlatTree& to);

}
