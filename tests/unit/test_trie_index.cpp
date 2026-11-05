#include <random>

#include "core/sha256.hpp"
#include "ds/trie.hpp"
#include "framework.hpp"
#include "store/index.hpp"
#include "util/fs.hpp"

using namespace chr;

std::string makeTempDir();

static HexTrie::Result oracle(const std::vector<Id>& ids, const std::string& prefix) {
  HexTrie::Result r;
  for (const Id& id : ids)
    if (startsWith(id.hex(), prefix)) {
      ++r.count;
      r.id = id;
    }
  r.kind = r.count == 0 ? HexTrie::Kind::None : r.count == 1 ? HexTrie::Kind::Unique : HexTrie::Kind::Ambiguous;
  return r;
}

TEST(trie_matches_linear_oracle) {
  std::mt19937 rng(17);
  HexTrie trie;
  std::vector<Id> ids;
  for (int i = 0; i < 3000; ++i) {
    Id id = sha256("obj" + std::to_string(i));
    if (i % 10 == 0) id = sha256("obj" + std::to_string(i / 2));
    trie.insert(id);
    bool dup = false;
    for (const Id& x : ids)
      if (x == id) dup = true;
    if (!dup) ids.push_back(id);
    if (i % 500 == 0) CHECK(trie.checkInvariants());
  }
  CHECK(trie.checkInvariants());
  CHECK_EQ(trie.size(), ids.size());
  int mismatches = 0;
  for (int i = 0; i < 4000; ++i) {
    std::string p;
    if (i % 2 == 0) {
      const Id& id = ids[rng() % ids.size()];
      p = id.hex().substr(0, 1 + rng() % 8);
    } else {
      for (size_t k = 0, n = 1 + rng() % 5; k < n; ++k) p += "0123456789abcdef"[rng() % 16];
    }
    HexTrie::Result a = trie.resolve(p), b = oracle(ids, p);
    if (a.kind != b.kind || a.count != b.count) ++mismatches;
    if (a.kind == HexTrie::Kind::Unique && !(a.id == b.id)) ++mismatches;
    if (a.kind == HexTrie::Kind::Ambiguous && a.candidates.size() != std::min<size_t>(10, b.count)) ++mismatches;
  }
  CHECK_EQ(mismatches, 0);
  for (int i = 0; i < 200; ++i) {
    const Id& id = ids[rng() % ids.size()];
    size_t L = trie.uniquePrefixLen(id, 1);
    CHECK_EQ(oracle(ids, id.hex().substr(0, L)).count, 1u);
    if (L > 1) CHECK(oracle(ids, id.hex().substr(0, L - 1)).count > 1);
    CHECK(trie.uniquePrefixLen(id, 8) >= 8);
  }
}

TEST(trie_empty_and_bad_input) {
  HexTrie t;
  CHECK(t.resolve("abcd").kind == HexTrie::Kind::None);
  CHECK(t.checkInvariants());
  t.insert(sha256("a"));
  CHECK(t.resolve("").kind == HexTrie::Kind::Unique);
  CHECK(t.resolve("XYZ!").kind == HexTrie::Kind::None);
  CHECK(t.checkInvariants());
}

TEST(index_round_trip_and_checksum) {
  std::string dir = makeTempDir();
  Index idx;
  std::vector<std::string> paths = {"a.txt", "dir/b c.txt", "dir/sub/new\nline", "tab\there", "pct%name",
                                    "a-b", "a/z"};
  int k = 0;
  for (const std::string& p : paths) {
    IndexEntry e;
    e.mode = (k % 2) ? kModeExec : kModeFile;
    e.id = sha256(p);
    e.size = static_cast<uint64_t>(k * 100);
    e.mtimeNs = 1700000000123456789LL + k;
    idx.entries[p] = e;
    ++k;
  }
  CHECK_OK(idx.save(dir + "/index"));
  Index back;
  CHECK_OK(back.load(dir + "/index"));
  CHECK_EQ(back.entries.size(), paths.size());
  for (const auto& kv : idx.entries) {
    auto it = back.entries.find(kv.first);
    CHECK(it != back.entries.end());
    if (it == back.entries.end()) continue;
    CHECK(it->second.id == kv.second.id);
    CHECK_EQ(it->second.mode, kv.second.mode);
    CHECK_EQ(it->second.size, kv.second.size);
    CHECK_EQ(it->second.mtimeNs, kv.second.mtimeNs);
  }
  std::string text;
  CHECK_OK(fsu::readFile(dir + "/index", &text));
  std::string bad = text;
  bad[20] = bad[20] == 'x' ? 'y' : 'x';
  CHECK_OK(fsu::writeFile(dir + "/index", bad));
  Status s = back.load(dir + "/index");
  CHECK(!s.ok());
  CHECK(s.msg.find("reset --hard") != std::string::npos);
  CHECK_OK(fsu::writeFile(dir + "/index", text.substr(0, text.size() / 2)));
  CHECK(!back.load(dir + "/index").ok());
  fsu::removeTree(dir);
}

TEST(index_build_tree_groups_directories) {
  std::string dir = makeTempDir();
  ObjectStore store;
  store.setDir(dir + "/objects");
  Index idx;
  for (const char* p : {"a-b", "a/x", "a/y/z", "b"}) idx.entries[p] = IndexEntry{kModeFile, sha256(p), 1, 0};
  Id root;
  CHECK_OK(idx.buildTree(store, &root));
  std::vector<TreeEntry> top;
  CHECK_OK(store.readTree(root, &top));
  CHECK_EQ(top.size(), 3u);
  CHECK_EQ(top[0].name, std::string("a"));
  CHECK(top[0].isDir());
  CHECK_EQ(top[1].name, std::string("a-b"));
  std::vector<TreeEntry> sub;
  CHECK_OK(store.readTree(top[0].id, &sub));
  CHECK_EQ(sub.size(), 2u);
  CHECK(sub[1].isDir());
  CHECK_EQ(idx.eraseTree("a"), 2u);
  CHECK_EQ(idx.entries.size(), 2u);
  fsu::removeTree(dir);
}
