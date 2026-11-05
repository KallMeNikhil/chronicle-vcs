#include "engine/treediff.hpp"

#include <algorithm>

#include "core/strutil.hpp"

namespace chr {

static Status addAll(ObjectStore& store, const TreeEntry& e, const std::string& path, char kind,
                     std::vector<TreeChange>* out) {
  if (!e.isDir()) {
    TreeChange c{kind, path, {}, {}};
    (kind == 'D' ? c.oldE : c.newE) = FlatEntry{e.mode, e.id};
    out->push_back(c);
    return Status::Ok();
  }
  FlatTree flat;
  CHR_TRY(flattenTree(store, e.id, &flat, path + "/"));
  for (const auto& kv : flat) {
    TreeChange c{kind, kv.first, {}, {}};
    (kind == 'D' ? c.oldE : c.newE) = kv.second;
    out->push_back(c);
  }
  return Status::Ok();
}

Status diffTrees(ObjectStore& store, const Id* a, const Id* b, std::vector<TreeChange>* out,
                 const std::string& prefix) {
  if (a && b && *a == *b) return Status::Ok();
  std::vector<TreeEntry> ea, eb;
  if (a) CHR_TRY(store.readTree(*a, &ea));
  if (b) CHR_TRY(store.readTree(*b, &eb));
  std::vector<TreeChange> local;
  size_t i = 0, j = 0;
  while (i < ea.size() || j < eb.size()) {
    if (j == eb.size() || (i < ea.size() && ea[i].name < eb[j].name)) {
      CHR_TRY(addAll(store, ea[i], prefix + ea[i].name, 'D', &local));
      ++i;
    } else if (i == ea.size() || eb[j].name < ea[i].name) {
      CHR_TRY(addAll(store, eb[j], prefix + eb[j].name, 'A', &local));
      ++j;
    } else {
      const TreeEntry &x = ea[i], &y = eb[j];
      std::string path = prefix + x.name;
      if (x.isDir() && y.isDir()) {
        CHR_TRY(diffTrees(store, &x.id, &y.id, &local, path + "/"));
      } else if (x.isDir() != y.isDir()) {
        CHR_TRY(addAll(store, x, path, 'D', &local));
        CHR_TRY(addAll(store, y, path, 'A', &local));
      } else if (!(x == y)) {
        local.push_back({'M', path, FlatEntry{x.mode, x.id}, FlatEntry{y.mode, y.id}});
      }
      ++i, ++j;
    }
  }
  std::stable_sort(local.begin(), local.end(),
                   [](const TreeChange& p, const TreeChange& q) { return p.path < q.path; });
  out->insert(out->end(), local.begin(), local.end());
  return Status::Ok();
}

std::vector<TreeChange> diffFlat(const FlatTree& a, const FlatTree& b) {
  std::vector<TreeChange> out;
  auto i = a.begin();
  auto j = b.begin();
  while (i != a.end() || j != b.end()) {
    if (j == b.end() || (i != a.end() && i->first < j->first)) {
      out.push_back({'D', i->first, i->second, {}});
      ++i;
    } else if (i == a.end() || j->first < i->first) {
      out.push_back({'A', j->first, {}, j->second});
      ++j;
    } else {
      if (i->second != j->second) out.push_back({'M', i->first, i->second, j->second});
      ++i, ++j;
    }
  }
  return out;
}

Status lookupPath(ObjectStore& store, const Id& root, const std::string& path, TreeEntry* out, bool* found) {
  *found = false;
  if (path.empty()) {
    *out = TreeEntry{"", kModeDir, root};
    *found = true;
    return Status::Ok();
  }
  Id cur = root;
  std::vector<std::string> parts = split(path, '/');
  for (size_t k = 0; k < parts.size(); ++k) {
    std::vector<TreeEntry> entries;
    CHR_TRY(store.readTree(cur, &entries));
    auto it = std::lower_bound(entries.begin(), entries.end(), parts[k],
                               [](const TreeEntry& e, const std::string& n) { return e.name < n; });
    if (it == entries.end() || it->name != parts[k]) return Status::Ok();
    if (k + 1 == parts.size()) {
      *out = *it;
      *found = true;
      return Status::Ok();
    }
    if (!it->isDir()) return Status::Ok();
    cur = it->id;
  }
  return Status::Ok();
}

}
