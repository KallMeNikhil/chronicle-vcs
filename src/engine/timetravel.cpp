#include "engine/timetravel.hpp"

#include <algorithm>
#include <climits>
#include <unordered_map>
#include <unordered_set>

#include "core/strutil.hpp"
#include "core/timeutil.hpp"
#include "engine/logwalk.hpp"
#include "engine/revparse.hpp"
#include "util/fs.hpp"

namespace chr {

Status commitAtTime(Repo& repo, int64_t t, Id* commit) {
  Id head;
  CHR_TRY(resolveRevision(repo, "HEAD", &head));
  std::vector<std::pair<Id, int64_t>> chain;
  for (Id cur = head;;) {
    Commit c;
    CHR_TRY(repo.store.readCommit(cur, &c));
    chain.push_back({cur, c.time});
    if (c.parents.empty()) break;
    cur = c.parents[0];
  }
  std::vector<std::pair<TimeKey, int>> keys;
  for (size_t i = 0; i < chain.size(); ++i) {
    int32_t pos = static_cast<int32_t>(chain.size() - 1 - i);
    keys.push_back({TimeKey{chain[i].second, pos}, static_cast<int>(i)});
  }
  std::sort(keys.begin(), keys.end());
  BPlusTree<TimeKey, int> tree(64);
  tree.bulkLoad(keys);
  auto it = tree.floor(TimeKey{t, INT32_MAX});
  if (!it.valid()) return Err("no commit at or before " + std::to_string(t) + " on the current branch");
  *commit = chain[static_cast<size_t>(tree.value(it))].first;
  return Status::Ok();
}

Status resolveRevOrTime(Repo& repo, const std::string& spec, Id* commit) {
  int64_t t = 0;
  bool isTime = parseTimeSpec(spec, &t);
  if (isTime && spec.find('-') != std::string::npos) return commitAtTime(repo, t, commit);
  Status s = resolveRevision(repo, spec, commit);
  if (s.ok() || !isTime) return s;
  return commitAtTime(repo, t, commit);
}

void buildTimeIndex(const Dag& dag, BPlusTree<TimeKey, int>* tree) {
  std::vector<std::pair<TimeKey, int>> keys;
  keys.reserve(static_cast<size_t>(dag.size()));
  for (int i = 0; i < dag.size(); ++i) keys.push_back({TimeKey{dag.node(i).time, i}, i});
  std::sort(keys.begin(), keys.end());
  tree->bulkLoad(keys);
}

static Status exportDir(Repo& repo, const Id& tree, const std::string& dir, size_t* files) {
  std::vector<TreeEntry> entries;
  CHR_TRY(repo.store.readTree(tree, &entries));
  for (const TreeEntry& e : entries) {
    std::string p = dir + "/" + e.name;
    if (e.isDir()) {
      CHR_TRY(fsu::mkdirs(p));
      CHR_TRY(exportDir(repo, e.id, p, files));
      continue;
    }
    std::string content;
    CHR_TRY(repo.store.readBlob(e.id, &content));
    if (e.mode == kModeLink) {
      CHR_TRY(fsu::makeSymlink(content, p));
    } else {
      CHR_TRY(fsu::writeFile(p, content, e.mode == kModeExec ? 0555 : 0444));
    }
    ++*files;
  }
  return Status::Ok();
}

Status exportTree(Repo& repo, const Id& commit, const std::string& dir, size_t* files) {
  fsu::FileStat st = fsu::lstatPath(dir);
  if (st.exists) {
    std::vector<std::string> names;
    if (!st.isDir || !fsu::listDir(dir, &names).ok() || !names.empty())
      return Err("export target '" + dir + "' must not exist or be an empty directory");
  }
  CHR_TRY(fsu::mkdirs(dir));
  Commit c;
  CHR_TRY(repo.store.readCommit(commit, &c));
  *files = 0;
  return exportDir(repo, c.tree, dir, files);
}

static Status countEntries(ObjectStore& store, const Id& tree, std::unordered_map<Id, uint64_t, IdHash>* memo,
                           uint64_t* out) {
  auto it = memo->find(tree);
  if (it != memo->end()) {
    *out = it->second;
    return Status::Ok();
  }
  std::vector<TreeEntry> entries;
  CHR_TRY(store.readTree(tree, &entries));
  uint64_t n = entries.size();
  for (const TreeEntry& e : entries) {
    if (!e.isDir()) continue;
    uint64_t sub = 0;
    CHR_TRY(countEntries(store, e.id, memo, &sub));
    n += sub;
  }
  (*memo)[tree] = n;
  *out = n;
  return Status::Ok();
}

static Status collectUnique(ObjectStore& store, const Id& tree, std::unordered_set<Id, IdHash>* seen) {
  if (!seen->insert(tree).second) return Status::Ok();
  std::vector<TreeEntry> entries;
  CHR_TRY(store.readTree(tree, &entries));
  for (const TreeEntry& e : entries) {
    if (e.isDir()) CHR_TRY(collectUnique(store, e.id, seen));
    else seen->insert(e.id);
  }
  return Status::Ok();
}

Status sharingReport(Repo& repo, size_t lastN, SharingReport* out) {
  *out = SharingReport();
  Id head;
  CHR_TRY(resolveRevision(repo, "HEAD", &head));
  Dag dag;
  CHR_TRY(dag.load(repo.store, {head}));
  std::vector<int> order = dateOrderWalk(dag, {dag.idxOf(head)});
  if (order.size() > lastN) order.resize(lastN);
  std::unordered_map<Id, uint64_t, IdHash> memo;
  std::unordered_set<Id, IdHash> unique;
  for (int x : order) {
    Commit c;
    CHR_TRY(repo.store.readCommit(dag.node(x).id, &c));
    uint64_t n = 0;
    CHR_TRY(countEntries(repo.store, c.tree, &memo, &n));
    out->snapshotEntries += n + 1;
    CHR_TRY(collectUnique(repo.store, c.tree, &unique));
  }
  out->commits = order.size();
  out->uniqueObjects = unique.size();
  return Status::Ok();
}

}
