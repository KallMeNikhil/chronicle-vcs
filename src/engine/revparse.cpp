#include "engine/revparse.hpp"

#include "core/strutil.hpp"

namespace chr {

Status buildIdTrie(ObjectStore& store, HexTrie* trie) {
  return store.forEachId([&](const Id& id) {
    trie->insert(id);
    return Status::Ok();
  });
}

std::string shortIdFor(const HexTrie& trie, const Id& id, size_t minLen) {
  return id.hex().substr(0, trie.uniquePrefixLen(id, minLen));
}

static Status resolveAbbrev(Repo& repo, const std::string& prefix, Id* out) {
  HexTrie trie;
  CHR_TRY(buildIdTrie(repo.store, &trie));
  HexTrie::Result r = trie.resolve(prefix);
  if (r.kind == HexTrie::Kind::Unique) {
    *out = r.id;
    return Status::Ok();
  }
  if (r.kind == HexTrie::Kind::None) return Err("unknown revision or object '" + prefix + "'");
  std::string msg = "ambiguous id prefix '" + prefix + "' (" + std::to_string(r.count) + " candidates):";
  for (const Id& c : r.candidates) {
    ObjType t = ObjType::None;
    std::string payload;
    if (!repo.store.get(c, &t, &payload).ok()) t = ObjType::None;
    msg += "\n  " + c.hex() + " " + typeName(t);
  }
  return Err(msg);
}

static Status resolveBase(Repo& repo, const std::string& base, Id* out) {
  if (base == "HEAD" || base == "@") {
    HeadInfo h;
    CHR_TRY(repo.refs.readHead(&h));
    if (h.unborn) return Err("HEAD does not point to a commit yet (branch '" + h.branch + "' has no commits)");
    *out = h.id;
    return Status::Ok();
  }
  bool hex = isLowerHex(base);
  if (base.size() == 64 && hex) {
    if (!Id::fromHex(base, out)) return Err("bad id '" + base + "'");
    if (!repo.store.has(*out)) return CorruptErr("missing object " + base);
    return Status::Ok();
  }
  if (repo.refs.branchExists(base)) return repo.refs.readBranch(base, out);
  if (hex && base.size() >= HexTrie::kMinPrefix) return resolveAbbrev(repo, base, out);
  if (hex) return Err("id prefix '" + base + "' is too short (minimum 4 hex characters)");
  return Err("unknown revision '" + base + "'");
}

static Status requireCommit(Repo& repo, const Id& id, const std::string& spec, Commit* c) {
  ObjType t;
  std::string payload;
  CHR_TRY(repo.store.get(id, &t, &payload));
  if (t != ObjType::Commit) return Err("'" + spec + "' names a " + typeName(t) + ", not a commit");
  Status s = decodeCommit(payload, c);
  if (!s.ok()) return CorruptErr("commit " + id.hex() + ": " + s.msg);
  return Status::Ok();
}

static Status applySuffixes(Repo& repo, const std::string& spec, size_t pos, Id* cur) {
  while (pos < spec.size()) {
    char op = spec[pos++];
    size_t start = pos;
    while (pos < spec.size() && spec[pos] >= '0' && spec[pos] <= '9') ++pos;
    int64_t n = 1;
    if (pos > start && !parseInt64(spec.substr(start, pos - start), &n)) return Err("bad number in '" + spec + "'");
    Commit c;
    CHR_TRY(requireCommit(repo, *cur, spec, &c));
    if (op == '~') {
      for (int64_t i = 0; i < n; ++i) {
        if (c.parents.empty()) return Err("'" + spec + "': ran out of first parents");
        *cur = c.parents[0];
        if (i + 1 < n) CHR_TRY(requireCommit(repo, *cur, spec, &c));
      }
    } else if (op == '^') {
      if (n == 0) continue;
      if (static_cast<size_t>(n) > c.parents.size())
        return Err("'" + spec + "': commit has no parent number " + std::to_string(n));
      *cur = c.parents[static_cast<size_t>(n - 1)];
    } else {
      return Err("bad revision syntax '" + spec + "'");
    }
  }
  return Status::Ok();
}

Status resolveAnyObject(Repo& repo, const std::string& spec, Id* out) {
  size_t cut = spec.find_first_of("~^");
  if (cut == 0 || spec.empty()) return Err("bad revision '" + spec + "'");
  CHR_TRY(resolveBase(repo, spec.substr(0, cut == std::string::npos ? spec.size() : cut), out));
  if (cut != std::string::npos) CHR_TRY(applySuffixes(repo, spec, cut, out));
  return Status::Ok();
}

Status resolveRevision(Repo& repo, const std::string& spec, Id* out) {
  CHR_TRY(resolveAnyObject(repo, spec, out));
  Commit c;
  return requireCommit(repo, *out, spec, &c);
}

}
