#include "engine/history.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>

#include "core/strutil.hpp"
#include "engine/logwalk.hpp"
#include "engine/treediff.hpp"
#include "util/fs.hpp"

namespace chr {

static const char* kHeader = "CHRONICLE-HISTORY 1\n";

Status commitEvents(ObjectStore& store, const Id& commit, std::vector<HistEvent>* out) {
  out->clear();
  Commit c;
  CHR_TRY(store.readCommit(commit, &c));
  Id parentTree;
  if (!c.parents.empty()) CHR_TRY(commitTreeId(store, c.parents[0], &parentTree));
  std::vector<TreeChange> changes;
  CHR_TRY(diffTrees(store, c.parents.empty() ? nullptr : &parentTree, &c.tree, &changes));
  for (const TreeChange& ch : changes) {
    HistEvent e{commit, ch.kind, 0, Id(), ch.path};
    if (ch.kind == 'D') {
      e.mode = ch.oldE.mode;
    } else {
      e.mode = ch.newE.mode;
      e.blob = ch.newE.id;
    }
    out->push_back(e);
  }
  return Status::Ok();
}

static std::string formatEvent(const HistEvent& e) {
  return e.commit.hex() + "\t" + std::string(1, e.kind) + "\t" + modeString(e.mode) + "\t" +
         (e.kind == 'D' ? std::string("-") : e.blob.hex()) + "\t" + escapePath(e.path) + "\n";
}

static Status appendText(const std::string& path, const std::string& text) {
  int fd = open(path.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0644);
  if (fd < 0) return Err("cannot append to " + path);
  ssize_t n = write(fd, text.data(), text.size());
  bool ok = n == static_cast<ssize_t>(text.size()) && fsync(fd) == 0;
  close(fd);
  return ok ? Status::Ok() : Err("cannot append to " + path);
}

static bool fileUsable(const std::string& path) {
  std::string text;
  if (!fsu::readFile(path, &text).ok()) return false;
  return startsWith(text, kHeader) && text.back() == '\n';
}

Status reindexHistory(Repo& repo, size_t* count) {
  std::vector<Id> tips;
  CHR_TRY(refTips(repo, &tips));
  Dag dag;
  CHR_TRY(dag.load(repo.store, tips));
  std::vector<int> tipIdx;
  for (const Id& t : tips) tipIdx.push_back(dag.idxOf(t));
  std::vector<int> order = dateOrderWalk(dag, tipIdx);
  std::reverse(order.begin(), order.end());
  std::string out = kHeader;
  size_t n = 0;
  for (int x : order) {
    std::vector<HistEvent> ev;
    CHR_TRY(commitEvents(repo.store, dag.node(x).id, &ev));
    for (const HistEvent& e : ev) out += formatEvent(e);
    n += ev.size();
  }
  if (count) *count = n;
  return fsu::writeFileAtomic(repo.path("history.idx"), out);
}

Status appendHistory(Repo& repo, const Id& commit) {
  std::string path = repo.path("history.idx");
  if (!fileUsable(path)) return reindexHistory(repo, nullptr);
  std::string existing;
  CHR_TRY(fsu::readFile(path, &existing));
  if (existing.find("\n" + commit.hex() + "\t") != std::string::npos) return Status::Ok();
  std::vector<HistEvent> ev;
  CHR_TRY(commitEvents(repo.store, commit, &ev));
  std::string text;
  for (const HistEvent& e : ev) text += formatEvent(e);
  return text.empty() ? Status::Ok() : appendText(path, text);
}

void HistoryIndex::addEvent(const HistEvent& e) {
  int i = static_cast<int>(events_.size());
  events_.push_back(e);
  byPath_[e.path].push_back(i);
  if (e.kind != 'D') byBlob_[e.blob].push_back(i);
  commits_.insert(e.commit);
}

Status HistoryIndex::load(Repo& repo) {
  events_.clear();
  byPath_.clear();
  byBlob_.clear();
  commits_.clear();
  std::string path = repo.path("history.idx");
  if (!fileUsable(path)) CHR_TRY(reindexHistory(repo, nullptr));
  std::string text;
  CHR_TRY(fsu::readFile(path, &text));
  std::vector<std::string> lines = split(text, '\n');
  for (size_t i = 1; i < lines.size(); ++i) {
    if (lines[i].empty()) continue;
    std::vector<std::string> f = split(lines[i], '\t');
    HistEvent e;
    if (f.size() != 5 || !Id::fromHex(f[0], &e.commit) || f[1].size() != 1 || !parseMode(f[2], &e.mode) ||
        !unescapePath(f[4], &e.path))
      return CorruptErr("history.idx line " + std::to_string(i + 1) + " is malformed (run 'chronicle reindex')");
    e.kind = f[1][0];
    if (e.kind != 'D' && !Id::fromHex(f[3], &e.blob))
      return CorruptErr("history.idx line " + std::to_string(i + 1) + " has a bad id (run 'chronicle reindex')");
    addEvent(e);
  }
  return Status::Ok();
}

Status HistoryIndex::ensureCovered(Repo& repo, const Dag& dag) {
  std::string text;
  for (int i = 0; i < dag.size(); ++i) {
    const Id& id = dag.node(i).id;
    if (commits_.count(id)) continue;
    std::vector<HistEvent> ev;
    CHR_TRY(commitEvents(repo.store, id, &ev));
    for (const HistEvent& e : ev) {
      addEvent(e);
      text += formatEvent(e);
    }
  }
  return text.empty() ? Status::Ok() : appendText(repo.path("history.idx"), text);
}

const std::vector<int>* HistoryIndex::forPath(const std::string& path) const {
  auto it = byPath_.find(path);
  return it == byPath_.end() ? nullptr : &it->second;
}

const std::vector<int>* HistoryIndex::forBlob(const Id& blob) const {
  auto it = byBlob_.find(blob);
  return it == byBlob_.end() ? nullptr : &it->second;
}

std::vector<int> HistoryIndex::forPrefix(const std::string& dir) const {
  std::vector<int> out;
  if (const std::vector<int>* exact = forPath(dir)) out = *exact;
  std::string pre = dir + "/";
  for (const auto& kv : byPath_)
    if (startsWith(kv.first, pre)) out.insert(out.end(), kv.second.begin(), kv.second.end());
  std::sort(out.begin(), out.end());
  return out;
}

}
