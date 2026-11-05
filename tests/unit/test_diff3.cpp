#include "core/sha256.hpp"
#include "engine/diff.hpp"
#include "engine/merge.hpp"
#include "framework.hpp"
#include "util/fs.hpp"

using namespace chr;

std::string makeTempDir();

static std::vector<std::string> L(const std::string& s) { return splitLines(s); }
static std::string merged(const std::string& b, const std::string& o, const std::string& t, int* conflicts) {
  Diff3Result r = diff3(L(b), L(o), L(t), "main", "topic");
  *conflicts = r.conflicts;
  return joinLines(r.lines);
}

TEST(diff3_independent_changes_merge) {
  int c = 0;
  CHECK_EQ(merged("1\n2\n3\n4\n5\n", "ONE\n2\n3\n4\n5\n", "1\n2\n3\n4\nFIVE\n", &c),
           std::string("ONE\n2\n3\n4\nFIVE\n"));
  CHECK_EQ(c, 0);
}

TEST(diff3_touching_regions_are_independent) {
  int c = 0;
  CHECK_EQ(merged("a\nb\nc\nd\n", "a\nB\nc\nd\n", "a\nb\nC\nd\n", &c), std::string("a\nB\nC\nd\n"));
  CHECK_EQ(c, 0);
}

TEST(diff3_same_position_inserts_conflict) {
  int c = 0;
  std::string out = merged("a\nb\n", "a\nX\nb\n", "a\nY\nb\n", &c);
  CHECK_EQ(c, 1);
  CHECK_EQ(out, std::string("a\n<<<<<<< ours (main)\nX\n=======\nY\n>>>>>>> theirs (topic)\nb\n"));
}

TEST(diff3_identical_changes_merge_cleanly) {
  int c = 0;
  CHECK_EQ(merged("a\nb\nc\n", "a\nZ\nc\n", "a\nZ\nc\n", &c), std::string("a\nZ\nc\n"));
  CHECK_EQ(c, 0);
  CHECK_EQ(merged("a\n", "a\nnew\n", "a\nnew\n", &c), std::string("a\nnew\n"));
  CHECK_EQ(c, 0);
}

TEST(diff3_overlapping_edits_conflict) {
  int c = 0;
  std::string out = merged("a\nb\nc\n", "a\nB1\nc\n", "a\nB2\nc\n", &c);
  CHECK_EQ(c, 1);
  CHECK(out.find("B1\n=======\nB2\n") != std::string::npos);
  out = merged("a\nb\nc\n", "a\nc\n", "a\nBB\nc\n", &c);
  CHECK_EQ(c, 1);
}

TEST(diff3_against_empty_base_and_missing_newline) {
  int c = 0;
  merged("", "x\n", "y\n", &c);
  CHECK_EQ(c, 1);
  std::string out = merged("a\nb", "a\nb", "a\nb\n", &c);
  CHECK_EQ(c, 0);
  CHECK_EQ(out, std::string("a\nb\n"));
  out = merged("a\n", "a\nx", "a\ny", &c);
  CHECK(out.find("x\n=======\ny\n>>>>>>>") != std::string::npos);
}

namespace {
struct T {
  std::string dir = makeTempDir();
  ObjectStore store;
  T() { store.setDir(dir + "/objects"); }
  ~T() { fsu::removeTree(dir); }
  Id blob(const std::string& s) {
    Id id;
    store.put(ObjType::Blob, s, &id);
    return id;
  }
  Id tree(std::vector<TreeEntry> e) {
    Id id;
    store.put(ObjType::Tree, encodeTree(e), &id);
    return id;
  }
};
}

TEST(tree_merge_rules) {
  T t;
  Id keep = t.blob("keep\n");
  Id base = t.tree({{"same", kModeFile, keep},
                    {"ours-only", kModeFile, t.blob("o\n")},
                    {"theirs-only", kModeFile, t.blob("t\n")},
                    {"both", kModeFile, t.blob("1\n2\n3\n")},
                    {"moddel", kModeFile, t.blob("m\n")},
                    {"modechg", kModeFile, t.blob("x\n")}});
  Id ours = t.tree({{"same", kModeFile, keep},
                    {"ours-only", kModeFile, t.blob("O\n")},
                    {"theirs-only", kModeFile, t.blob("t\n")},
                    {"both", kModeFile, t.blob("ONE\n2\n3\n")},
                    {"modechg", kModeExec, t.blob("x\n")},
                    {"addadd", kModeFile, t.blob("ours\n")}});
  Id theirs = t.tree({{"same", kModeFile, keep},
                      {"ours-only", kModeFile, t.blob("o\n")},
                      {"theirs-only", kModeFile, t.blob("T\n")},
                      {"both", kModeFile, t.blob("1\n2\nTHREE\n")},
                      {"moddel", kModeFile, t.blob("M2\n")},
                      {"modechg", kModeFile, t.blob("x\n")},
                      {"addadd", kModeFile, t.blob("theirs\n")}});
  MergeOutcome m;
  CHECK_OK(mergeTrees(t.store, &base, &ours, &theirs, "main", "topic", &m));
  CHECK(m.index["ours-only"].id == t.blob("O\n"));
  CHECK(m.index["theirs-only"].id == t.blob("T\n"));
  CHECK(m.index["both"].id == t.blob("ONE\n2\nTHREE\n"));
  CHECK_EQ(m.index["modechg"].mode, kModeExec);
  CHECK(m.index["moddel"].id == t.blob("M2\n"));
  CHECK_EQ(m.conflicts.size(), 2u);
  bool sawModDel = false, sawAddAdd = false;
  for (auto& c : m.conflicts) {
    if (c.first == "modify/delete" && c.second == "moddel") sawModDel = true;
    if (c.first == "add/add" && c.second == "addadd") sawAddAdd = true;
  }
  CHECK(sawModDel && sawAddAdd);
  CHECK(m.markerFiles.count("addadd") == 1);
  CHECK(m.index["addadd"].id == t.blob("ours\n"));
}

TEST(tree_merge_file_vs_directory_and_subtrees) {
  T t;
  Id sub = t.tree({{"x", kModeFile, t.blob("x\n")}});
  Id sub2 = t.tree({{"x", kModeFile, t.blob("x\n")}, {"y", kModeFile, t.blob("y\n")}});
  Id base = t.tree({{"p", kModeFile, t.blob("p\n")}, {"d", kModeDir, sub}});
  Id ours = t.tree({{"p", kModeFile, t.blob("p2\n")}, {"d", kModeDir, sub2}});
  Id theirs = t.tree({{"p", kModeDir, sub}, {"d", kModeDir, sub}});
  MergeOutcome m;
  CHECK_OK(mergeTrees(t.store, &base, &ours, &theirs, "main", "topic", &m));
  CHECK_EQ(m.conflicts.size(), 1u);
  if (!m.conflicts.empty()) CHECK_EQ(m.conflicts[0].first, std::string("file/directory"));
  CHECK(m.index["p"].id == t.blob("p2\n"));
  CHECK(m.index.count("d/y") == 1);
  Id a = t.tree({{"a", kModeFile, t.blob("a\n")}}), b = t.tree({{"b", kModeFile, t.blob("b\n")}});
  CHECK_OK(mergeTrees(t.store, nullptr, &a, &b, "main", "topic", &m));
  CHECK(m.clean());
  CHECK_EQ(m.index.size(), 2u);
}
