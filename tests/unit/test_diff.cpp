#include <random>

#include "core/sha256.hpp"
#include "engine/diff.hpp"
#include "engine/treediff.hpp"
#include "framework.hpp"
#include "util/fs.hpp"

using namespace chr;

std::string makeTempDir();

static std::vector<std::string> randomLines(std::mt19937& rng, int maxLines) {
  std::vector<std::string> v;
  int n = static_cast<int>(rng() % static_cast<unsigned>(maxLines + 1));
  for (int i = 0; i < n; ++i) v.push_back(std::string(1, static_cast<char>('a' + rng() % 4)) + "\n");
  if (!v.empty() && rng() % 3 == 0) v.back().pop_back();
  return v;
}

TEST(myers_apply_round_trip_10k) {
  std::mt19937 rng(2024);
  int bad = 0;
  for (int t = 0; t < 10000; ++t) {
    std::vector<std::string> a = randomLines(rng, 30), b;
    if (t % 2 == 0) {
      b = randomLines(rng, 30);
    } else {
      b = a;
      for (int k = 0, e = static_cast<int>(rng() % 5); k < e; ++k) {
        if (!b.empty() && rng() % 2) b.erase(b.begin() + static_cast<long>(rng() % b.size()));
        else b.insert(b.begin() + static_cast<long>(b.empty() ? 0 : rng() % b.size()), "z\n");
      }
    }
    std::vector<DiffOp> ops = diffLines(a, b);
    if (applyRegions(a, regionsFromOps(ops, b)) != b) ++bad;
    int na = 0, nb = 0;
    for (const DiffOp& o : ops) {
      if (o.kind == '=' && a[static_cast<size_t>(o.a)] != b[static_cast<size_t>(o.b)]) ++bad;
      if (o.kind != '+') ++na;
      if (o.kind != '-') ++nb;
    }
    if (na != static_cast<int>(a.size()) || nb != static_cast<int>(b.size())) ++bad;
  }
  CHECK_EQ(bad, 0);
}

TEST(myers_minimal_edit_distance) {
  std::vector<int> a = {1, 2, 3, 1, 2, 2, 1}, b = {3, 2, 1, 2, 1, 3};
  int d = 0;
  for (const DiffOp& o : diffSeq(a, b))
    if (o.kind != '=') ++d;
  CHECK_EQ(d, 5);
  CHECK_EQ(diffSeq({}, {}).size(), 0u);
  CHECK_EQ(diffSeq({}, {1, 2}).size(), 2u);
  std::vector<int> x(50), y(50);
  for (int i = 0; i < 50; ++i) x[static_cast<size_t>(i)] = i, y[static_cast<size_t>(i)] = 100 + i;
  CHECK_EQ(diffSeq(x, y, 3).size(), 100u);
}

TEST(final_newline_is_part_of_identity) {
  std::vector<std::string> a = splitLines("x\ny\n"), b = splitLines("x\ny");
  CHECK_EQ(a.size(), 2u);
  CHECK_EQ(b[1], std::string("y"));
  std::string h = unifiedHunks("x\ny\n", "x\ny");
  CHECK(h.find("-y\n") != std::string::npos);
  CHECK(h.find("+y\n\\ No newline at end of file\n") != std::string::npos);
  CHECK(isBinary(std::string("ab\0c", 4)));
  CHECK(!isBinary("plain text"));
}

TEST(unified_hunk_headers) {
  std::string a, b;
  for (int i = 1; i <= 20; ++i) {
    a += std::to_string(i) + "\n";
    b += (i == 10 ? std::string("ten") : std::to_string(i)) + "\n";
  }
  std::string h = unifiedHunks(a, b);
  CHECK(h.find("@@ -7,7 +7,7 @@\n") == 0);
  CHECK(h.find("-10\n+ten\n") != std::string::npos);
  CHECK_EQ(unifiedHunks("", "new\n"), std::string("@@ -0,0 +1,1 @@\n+new\n"));
  CHECK_EQ(unifiedHunks("old\n", ""), std::string("@@ -1,1 +0,0 @@\n-old\n"));
}

TEST(tree_diff_skips_equal_subtrees) {
  std::string dir = makeTempDir();
  ObjectStore store;
  store.setDir(dir + "/objects");
  auto blob = [&](const std::string& s) {
    Id id;
    store.put(ObjType::Blob, s, &id);
    return id;
  };
  auto tree = [&](std::vector<TreeEntry> e) {
    Id id;
    store.put(ObjType::Tree, encodeTree(e), &id);
    return id;
  };
  Id same = tree({{"x", kModeFile, blob("x")}});
  Id sub1 = tree({{"f", kModeFile, blob("1")}});
  Id sub2 = tree({{"f", kModeFile, blob("2")}, {"g", kModeFile, blob("g")}});
  Id t1 = tree({{"same", kModeDir, same}, {"sub", kModeDir, sub1}, {"gone", kModeFile, blob("q")}});
  Id t2 = tree({{"same", kModeDir, same}, {"sub", kModeDir, sub2}, {"new", kModeExec, blob("n")}});
  CHECK_OK(fsu::removeFile(store.loose().pathFor(same)));
  store.clearCache();
  std::vector<TreeChange> ch;
  CHECK_OK(diffTrees(store, &t1, &t2, &ch));
  CHECK_EQ(ch.size(), 4u);
  if (ch.size() == 4) {
    CHECK_EQ(ch[0].path, std::string("gone"));
    CHECK_EQ(ch[0].kind, 'D');
    CHECK_EQ(ch[1].path, std::string("new"));
    CHECK_EQ(ch[2].path, std::string("sub/f"));
    CHECK_EQ(ch[2].kind, 'M');
    CHECK_EQ(ch[3].path, std::string("sub/g"));
  }
  TreeEntry e;
  bool found = false;
  CHECK_OK(lookupPath(store, t2, "sub/g", &e, &found));
  CHECK(found && e.id == blob("g"));
  CHECK_OK(lookupPath(store, t2, "sub/zz", &e, &found));
  CHECK(!found);
  fsu::removeTree(dir);
}
