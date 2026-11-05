#include "engine/worktree.hpp"

#include <unistd.h>

#include <algorithm>

#include "core/strutil.hpp"

namespace chr {

Status flattenTree(ObjectStore& store, const Id& tree, FlatTree* out, const std::string& prefix) {
  std::vector<TreeEntry> entries;
  CHR_TRY(store.readTree(tree, &entries));
  for (const TreeEntry& e : entries) {
    if (e.isDir())
      CHR_TRY(flattenTree(store, e.id, out, prefix + e.name + "/"));
    else
      (*out)[prefix + e.name] = FlatEntry{e.mode, e.id};
  }
  return Status::Ok();
}

Status commitTreeId(ObjectStore& store, const Id& commit, Id* tree) {
  Commit c;
  CHR_TRY(store.readCommit(commit, &c));
  *tree = c.tree;
  return Status::Ok();
}

Status flattenCommit(ObjectStore& store, const Id& commit, FlatTree* out) {
  out->clear();
  Id tree;
  CHR_TRY(commitTreeId(store, commit, &tree));
  return flattenTree(store, tree, out);
}

Status headFlat(Repo& repo, FlatTree* out, HeadInfo* head) {
  HeadInfo h;
  CHR_TRY(repo.refs.readHead(&h));
  if (head) *head = h;
  out->clear();
  if (h.unborn) return Status::Ok();
  return flattenCommit(repo.store, h.id, out);
}

FlatTree indexFlat(const Index& index) {
  FlatTree f;
  for (const auto& kv : index.entries) f[kv.first] = FlatEntry{kv.second.mode, kv.second.id};
  return f;
}

static Status walkDir(const Repo& repo, const Ignore& ignore, const std::string& rel,
                      std::map<std::string, fsu::FileStat>* out, std::vector<std::string>* errors) {
  std::string abs = rel.empty() ? repo.work : repo.work + "/" + rel;
  std::vector<std::string> names;
  if (!fsu::listDir(abs, &names).ok()) {
    errors->push_back(rel.empty() ? "." : rel);
    return Status::Ok();
  }
  for (const std::string& n : names) {
    std::string r = rel.empty() ? n : rel + "/" + n;
    fsu::FileStat st = fsu::lstatPath(repo.work + "/" + r);
    if (st.isDir) {
      if (!ignore.ignored(r, true)) CHR_TRY(walkDir(repo, ignore, r, out, errors));
    } else if ((st.isReg || st.isLink) && !ignore.ignored(r, false)) {
      (*out)[r] = st;
    }
  }
  return Status::Ok();
}

Status walkWorktree(const Repo& repo, const Ignore& ignore, const std::string& startRel,
                    std::map<std::string, fsu::FileStat>* out, std::vector<std::string>* errors) {
  return walkDir(repo, ignore, startRel, out, errors);
}

Status hashWorkFile(Repo& repo, const std::string& rel, const fsu::FileStat& st, bool store,
                    IndexEntry* out) {
  std::string path = repo.work + "/" + rel;
  std::string content;
  if (st.isLink) {
    CHR_TRY(fsu::readLink(path, &content));
    out->mode = kModeLink;
  } else {
    CHR_TRY(fsu::readFile(path, &content));
    out->mode = st.exec ? kModeExec : kModeFile;
  }
  out->size = st.size;
  out->mtimeNs = st.mtimeNs;
  if (store) return repo.store.put(ObjType::Blob, content, &out->id);
  out->id = objectId(ObjType::Blob, content);
  return Status::Ok();
}

static uint32_t statMode(const fsu::FileStat& st) {
  if (st.isLink) return kModeLink;
  return st.exec ? kModeExec : kModeFile;
}

bool statClean(const IndexEntry& e, const fsu::FileStat& st, int64_t indexWriteNs) {
  return st.size == e.size && st.mtimeNs == e.mtimeNs && statMode(st) == e.mode &&
         e.mtimeNs < indexWriteNs - 2000000000LL;
}

Status workFileDiffers(Repo& repo, const std::string& rel, const IndexEntry& e, int64_t indexWriteNs,
                       bool* differs) {
  fsu::FileStat st = fsu::lstatPath(repo.work + "/" + rel);
  if (!st.exists || st.isDir) {
    *differs = true;
    return Status::Ok();
  }
  if (statClean(e, st, indexWriteNs)) {
    *differs = false;
    return Status::Ok();
  }
  if (statMode(st) != e.mode || (e.mode != kModeLink && st.size != e.size)) {
    *differs = true;
    return Status::Ok();
  }
  IndexEntry now;
  CHR_TRY(hashWorkFile(repo, rel, st, false, &now));
  *differs = !(now.id == e.id) || now.mode != e.mode;
  return Status::Ok();
}

Status computeStatus(Repo& repo, const Index& index, StatusReport* out, bool wantUntracked) {
  *out = StatusReport();
  FlatTree head;
  CHR_TRY(headFlat(repo, &head));
  auto h = head.begin();
  auto i = index.entries.begin();
  while (h != head.end() || i != index.entries.end()) {
    if (i == index.entries.end() || (h != head.end() && h->first < i->first)) {
      out->staged.push_back({'D', h->first});
      ++h;
    } else if (h == head.end() || i->first < h->first) {
      out->staged.push_back({'A', i->first});
      ++i;
    } else {
      if (h->second != FlatEntry{i->second.mode, i->second.id}) out->staged.push_back({'M', i->first});
      ++h;
      ++i;
    }
  }
  for (const auto& kv : index.entries) {
    fsu::FileStat st = fsu::lstatPath(repo.work + "/" + kv.first);
    if (!st.exists || st.isDir) {
      out->unstaged.push_back({'D', kv.first});
      continue;
    }
    bool differs = false;
    Status s = workFileDiffers(repo, kv.first, kv.second, index.writeTimeNs, &differs);
    if (!s.ok() || differs) out->unstaged.push_back({'M', kv.first});
  }
  if (!wantUntracked) return Status::Ok();
  Ignore ignore;
  CHR_TRY(ignore.load(repo.work));
  std::map<std::string, fsu::FileStat> files;
  std::vector<std::string> errors;
  CHR_TRY(walkWorktree(repo, ignore, "", &files, &errors));
  for (const auto& kv : files)
    if (!index.entries.count(kv.first)) out->untracked.push_back(kv.first);
  return Status::Ok();
}

static Status prepareParents(Repo& repo, const std::string& rel) {
  std::string cur = repo.work;
  std::vector<std::string> parts = split(rel, '/');
  for (size_t k = 0; k + 1 < parts.size(); ++k) {
    cur += "/" + parts[k];
    fsu::FileStat st = fsu::lstatPath(cur);
    if (st.isLink || st.isReg) CHR_TRY(fsu::removeFile(cur));
    if (!st.isDir) CHR_TRY(fsu::mkdirs(cur));
  }
  return Status::Ok();
}

Status writeWorkFile(Repo& repo, const std::string& rel, const FlatEntry& e, IndexEntry* stamped) {
  for (const std::string& comp : split(rel, '/'))
    if (!validEntryName(comp)) return Err("refusing to write unsafe path '" + escapePath(rel) + "'");
  std::string content;
  CHR_TRY(repo.store.readBlob(e.id, &content));
  CHR_TRY(prepareParents(repo, rel));
  std::string path = repo.work + "/" + rel;
  fsu::FileStat st = fsu::lstatPath(path);
  if (st.isDir) {
    if (rmdir(path.c_str()) != 0) return Err("cannot replace directory '" + rel + "' with a file");
  } else if (st.exists) {
    CHR_TRY(fsu::removeFile(path));
  }
  if (e.mode == kModeLink) {
    CHR_TRY(fsu::makeSymlink(content, path));
  } else {
    CHR_TRY(fsu::writeFile(path, content, e.mode == kModeExec ? 0755 : 0644));
    CHR_TRY(fsu::setExecutable(path, e.mode == kModeExec));
  }
  fsu::FileStat now = fsu::lstatPath(path);
  stamped->mode = e.mode;
  stamped->id = e.id;
  stamped->size = now.size;
  stamped->mtimeNs = now.mtimeNs;
  return Status::Ok();
}

Status removeWorkFile(Repo& repo, const std::string& rel) {
  std::string path = repo.work + "/" + rel;
  fsu::FileStat st = fsu::lstatPath(path);
  if (st.exists && !st.isDir) CHR_TRY(fsu::removeFile(path));
  fsu::removeEmptyParents(path, repo.work);
  return Status::Ok();
}

Status checkUntrackedOverwrite(Repo& repo, const Index& index, const FlatTree& from,
                                      const FlatTree& to) {
  std::vector<std::string> clobbered;
  for (const auto& kv : to) {
    auto f = from.find(kv.first);
    if (f != from.end() && f->second == kv.second) continue;
    std::string cur;
    for (const std::string& comp : split(kv.first, '/')) {
      cur = cur.empty() ? comp : cur + "/" + comp;
      fsu::FileStat st = fsu::lstatPath(repo.work + "/" + cur);
      if (!st.exists) break;
      bool last = cur.size() == kv.first.size();
      if (st.isDir && !last) continue;
      if (st.isDir && last) {
        if (index.hasPrefix(cur)) break;
        clobbered.push_back(cur + "/");
        break;
      }
      if (!index.entries.count(cur)) clobbered.push_back(cur);
      break;
    }
  }
  if (clobbered.empty()) return Status::Ok();
  std::string msg = "untracked working tree files would be overwritten:";
  for (const std::string& c : clobbered) msg += "\n  " + displayPath(c);
  return Err(msg);
}

Status switchTree(Repo& repo, Index& index, const FlatTree& from, const FlatTree& to) {
  StatusReport st;
  CHR_TRY(computeStatus(repo, index, &st, false));
  if (!st.clean()) {
    std::string msg = "your local changes would be lost; commit them or run 'chronicle reset --hard':";
    for (const auto& c : st.staged) msg += "\n  staged " + displayPath(c.second);
    for (const auto& c : st.unstaged) msg += "\n  modified " + displayPath(c.second);
    return Err(msg);
  }
  CHR_TRY(checkUntrackedOverwrite(repo, index, from, to));
  return applyTree(repo, index, from, to);
}

Status applyTree(Repo& repo, Index& index, const FlatTree& from, const FlatTree& to) {
  for (const auto& kv : from) {
    if (to.count(kv.first)) continue;
    CHR_TRY(removeWorkFile(repo, kv.first));
    index.entries.erase(kv.first);
  }
  for (const auto& kv : to) {
    auto f = from.find(kv.first);
    if (f != from.end() && f->second == kv.second && index.entries.count(kv.first)) continue;
    IndexEntry e;
    CHR_TRY(writeWorkFile(repo, kv.first, kv.second, &e));
    index.entries[kv.first] = e;
  }
  return Status::Ok();
}

Status hardResetTree(Repo& repo, Index& index, const FlatTree& tracked, const FlatTree& to) {
  for (const auto& kv : tracked)
    if (!to.count(kv.first)) CHR_TRY(removeWorkFile(repo, kv.first));
  Index fresh;
  for (const auto& kv : to) {
    auto old = index.entries.find(kv.first);
    bool same = false;
    if (old != index.entries.end() && FlatEntry{old->second.mode, old->second.id} == kv.second) {
      bool differs = true;
      CHR_TRY(workFileDiffers(repo, kv.first, old->second, index.writeTimeNs, &differs));
      same = !differs;
    }
    if (same) {
      fresh.entries[kv.first] = old->second;
      continue;
    }
    IndexEntry e;
    CHR_TRY(writeWorkFile(repo, kv.first, kv.second, &e));
    fresh.entries[kv.first] = e;
  }
  index.entries.swap(fresh.entries);
  return Status::Ok();
}

}
