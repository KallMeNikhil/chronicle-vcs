#include "engine/merge.hpp"

#include <algorithm>
#include <optional>
#include <set>

#include "engine/diff.hpp"

namespace chr {

namespace {

bool overlaps(int bs, int be, const Region& r) { return (r.bs < be && bs < r.be) || r.bs == bs; }

std::vector<std::string> sideText(const std::vector<std::string>& base, const std::vector<const Region*>& regs,
                                  int cs, int ce) {
  std::vector<std::string> out;
  int cur = cs;
  for (const Region* r : regs) {
    for (; cur < r->bs; ++cur) out.push_back(base[static_cast<size_t>(cur)]);
    out.insert(out.end(), r->lines.begin(), r->lines.end());
    cur = std::max(cur, r->be);
  }
  for (; cur < ce; ++cur) out.push_back(base[static_cast<size_t>(cur)]);
  return out;
}

void pushTerminated(std::vector<std::string>& out, const std::vector<std::string>& lines) {
  for (const std::string& l : lines) out.push_back(!l.empty() && l.back() == '\n' ? l : l + "\n");
}

}

Diff3Result diff3(const std::vector<std::string>& base, const std::vector<std::string>& ours,
                  const std::vector<std::string>& theirs, const std::string& oursLabel,
                  const std::string& theirsLabel) {
  std::vector<Region> A = regionsFromOps(diffLines(base, ours), ours);
  std::vector<Region> B = regionsFromOps(diffLines(base, theirs), theirs);
  Diff3Result res;
  size_t i = 0, j = 0;
  int pos = 0;
  while (i < A.size() || j < B.size()) {
    bool takeA = j >= B.size() || (i < A.size() && A[i].bs <= B[j].bs);
    const Region& seed = takeA ? A[i] : B[j];
    int cs = seed.bs, ce = seed.be;
    std::vector<const Region*> ra, rb;
    (takeA ? ra : rb).push_back(&seed);
    takeA ? ++i : ++j;
    for (bool grew = true; grew;) {
      grew = false;
      if (i < A.size() && overlaps(cs, ce, A[i])) {
        ra.push_back(&A[i]);
        ce = std::max(ce, A[i].be);
        ++i;
        grew = true;
      }
      if (j < B.size() && overlaps(cs, ce, B[j])) {
        rb.push_back(&B[j]);
        ce = std::max(ce, B[j].be);
        ++j;
        grew = true;
      }
    }
    for (; pos < cs; ++pos) res.lines.push_back(base[static_cast<size_t>(pos)]);
    std::vector<std::string> o = sideText(base, ra, cs, ce), t = sideText(base, rb, cs, ce);
    if (rb.empty() || ra.empty() || o == t) {
      const std::vector<std::string>& take = rb.empty() ? o : (ra.empty() ? t : o);
      res.lines.insert(res.lines.end(), take.begin(), take.end());
    } else {
      ++res.conflicts;
      res.lines.push_back("<<<<<<< ours (" + oursLabel + ")\n");
      pushTerminated(res.lines, o);
      res.lines.push_back("=======\n");
      pushTerminated(res.lines, t);
      res.lines.push_back(">>>>>>> theirs (" + theirsLabel + ")\n");
    }
    pos = ce;
  }
  for (; pos < static_cast<int>(base.size()); ++pos) res.lines.push_back(base[static_cast<size_t>(pos)]);
  return res;
}

namespace {

using Opt = std::optional<TreeEntry>;

struct TreeMerger {
  ObjectStore& store;
  std::string oursLabel, theirsLabel;
  MergeOutcome* out;

  static bool same(const Opt& a, const Opt& b) {
    if (!a || !b) return !a && !b;
    return a->mode == b->mode && a->id == b->id;
  }
  static bool isDir(const Opt& e) { return e && e->isDir(); }

  Status take(const std::string& path, const Opt& e) {
    if (!e) return Status::Ok();
    if (e->isDir()) return flattenTree(store, e->id, &out->index, path + "/");
    out->index[path] = FlatEntry{e->mode, e->id};
    return Status::Ok();
  }

  Status entries(const Opt& e, std::map<std::string, TreeEntry>* m) {
    if (!isDir(e)) return Status::Ok();
    std::vector<TreeEntry> v;
    CHR_TRY(store.readTree(e->id, &v));
    for (const TreeEntry& x : v) (*m)[x.name] = x;
    return Status::Ok();
  }

  Status recurse(const std::string& prefix, const Opt& b, const Opt& o, const Opt& t) {
    std::map<std::string, TreeEntry> mb, mo, mt;
    CHR_TRY(entries(b, &mb));
    CHR_TRY(entries(o, &mo));
    CHR_TRY(entries(t, &mt));
    std::set<std::string> names;
    for (auto* m : {&mb, &mo, &mt})
      for (const auto& kv : *m) names.insert(kv.first);
    for (const std::string& n : names) {
      auto get = [&](std::map<std::string, TreeEntry>& m) { return m.count(n) ? Opt(m[n]) : Opt(); };
      CHR_TRY(mergeEntry(prefix + n, get(mb), get(mo), get(mt)));
    }
    return Status::Ok();
  }

  Status mergeFile(const std::string& path, const Opt& b, const TreeEntry& o, const TreeEntry& t) {
    std::string bt, ot, tt;
    bool haveBase = b && !b->isDir();
    if (haveBase) CHR_TRY(store.readBlob(b->id, &bt));
    CHR_TRY(store.readBlob(o.id, &ot));
    CHR_TRY(store.readBlob(t.id, &tt));
    const char* kind = haveBase ? "content" : "add/add";
    uint32_t mb = haveBase ? b->mode : o.mode;
    uint32_t mode = o.mode == t.mode ? o.mode : (mb == o.mode ? t.mode : o.mode);
    bool modeConflict = o.mode != t.mode && mb != o.mode && mb != t.mode;
    if (isBinary(bt) || isBinary(ot) || isBinary(tt)) {
      out->index[path] = FlatEntry{o.mode, o.id};
      out->conflicts.push_back({"binary", path});
      return Status::Ok();
    }
    Diff3Result r = diff3(splitLines(bt), splitLines(ot), splitLines(tt), oursLabel, theirsLabel);
    std::string text = joinLines(r.lines);
    if (r.conflicts > 0) {
      out->index[path] = FlatEntry{o.mode, o.id};
      out->markerFiles[path] = text;
      out->conflicts.push_back({kind, path});
      return Status::Ok();
    }
    Id id;
    CHR_TRY(store.put(ObjType::Blob, text, &id));
    out->index[path] = FlatEntry{mode, id};
    if (modeConflict) out->conflicts.push_back({"mode", path});
    return Status::Ok();
  }

  Status mergeEntry(const std::string& path, const Opt& b, const Opt& o, const Opt& t) {
    if (same(o, t)) return take(path, o);
    if (same(b, o)) return take(path, t);
    if (same(b, t)) return take(path, o);
    if ((!o || isDir(o)) && (!t || isDir(t)))
      return recurse(path + "/", isDir(b) ? b : Opt(), o, t);
    if (o && t && !o->isDir() && !t->isDir()) return mergeFile(path, b, *o, *t);
    if (!o || !t) {
      const Opt& kept = o ? o : t;
      CHR_TRY(take(path, kept));
      out->conflicts.push_back({"modify/delete", path});
      return Status::Ok();
    }
    CHR_TRY(take(path, o));
    out->conflicts.push_back({"file/directory", path});
    return Status::Ok();
  }
};

}

Status mergeTrees(ObjectStore& store, const Id* base, const Id* ours, const Id* theirs,
                  const std::string& oursLabel, const std::string& theirsLabel, MergeOutcome* out) {
  *out = MergeOutcome();
  auto root = [](const Id* id) { return id ? Opt(TreeEntry{"", kModeDir, *id}) : Opt(); };
  TreeMerger m{store, oursLabel, theirsLabel, out};
  Opt b = root(base), o = root(ours), t = root(theirs);
  if (TreeMerger::same(o, t)) return ours ? flattenTree(store, *ours, &out->index) : Status::Ok();
  return m.recurse("", b, o, t);
}

Status applyMergeOutcome(Repo& repo, Index& index, const FlatTree& ours, const MergeOutcome& m) {
  CHR_TRY(checkUntrackedOverwrite(repo, index, ours, m.index));
  CHR_TRY(applyTree(repo, index, ours, m.index));
  for (const auto& kv : m.markerFiles) {
    std::string path = repo.work + "/" + kv.first;
    CHR_TRY(fsu::removeFile(path));
    const IndexEntry& e = index.entries[kv.first];
    CHR_TRY(fsu::writeFile(path, kv.second, e.mode == kModeExec ? 0755 : 0644));
    index.entries[kv.first].mtimeNs = 0;
  }
  return Status::Ok();
}

}
