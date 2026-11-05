#include <algorithm>

#include "engine/dag.hpp"
#include "engine/graphview.hpp"
#include "engine/logwalk.hpp"
#include "framework.hpp"
#include "util/fs.hpp"

using namespace chr;

std::string makeTempDir();

namespace {
struct Builder {
  std::string dir = makeTempDir();
  ObjectStore store;
  Id emptyTree;
  Builder() {
    store.setDir(dir + "/objects");
    store.put(ObjType::Tree, "", &emptyTree);
  }
  ~Builder() { fsu::removeTree(dir); }
  Id commit(const std::string& msg, int64_t time, std::vector<Id> parents) {
    Commit c;
    c.tree = emptyTree;
    c.parents = parents;
    c.author = "T <t@x>";
    c.time = time;
    c.message = msg + "\n";
    Id id;
    store.put(ObjType::Commit, encodeCommit(c), &id);
    return id;
  }
};

std::vector<Id> ids(const Dag& dag, const std::vector<int>& order) {
  std::vector<Id> out;
  for (int x : order) out.push_back(dag.node(x).id);
  return out;
}
}

TEST(dag_linear_log_and_generation) {
  Builder b;
  Id c1 = b.commit("1", 100, {});
  Id c2 = b.commit("2", 200, {c1});
  Id c3 = b.commit("3", 300, {c2});
  Dag dag;
  CHECK_OK(dag.load(b.store, {c3}));
  CHECK_EQ(dag.size(), 3);
  CHECK_EQ(dag.node(dag.idxOf(c1)).generation, 1);
  CHECK_EQ(dag.node(dag.idxOf(c3)).generation, 3);
  std::vector<Id> order = ids(dag, dateOrderWalk(dag, {dag.idxOf(c3)}));
  CHECK(order == (std::vector<Id>{c3, c2, c1}));
  CHECK(dag.isAncestor(dag.idxOf(c1), dag.idxOf(c3)));
  CHECK(!dag.isAncestor(dag.idxOf(c3), dag.idxOf(c1)));
  CHECK(dag.isAncestor(dag.idxOf(c2), dag.idxOf(c2)));
  std::vector<int> mb = dag.mergeBases(dag.idxOf(c2), dag.idxOf(c3));
  CHECK(mb.size() == 1 && dag.node(mb[0]).id == c2);
}

TEST(dag_log_respects_topology_under_clock_skew) {
  Builder b;
  Id root = b.commit("root", 1000, {});
  Id old = b.commit("child-with-old-clock", 10, {root});
  Id side = b.commit("side", 500, {root});
  Id merge = b.commit("merge", 600, {old, side});
  Dag dag;
  CHECK_OK(dag.load(b.store, {merge}));
  std::vector<Id> order = ids(dag, dateOrderWalk(dag, {dag.idxOf(merge)}));
  CHECK_EQ(order.size(), 4u);
  CHECK(order[0] == merge);
  CHECK(order[1] == side);
  CHECK(order[2] == old);
  CHECK(order[3] == root);
  std::vector<Id> naive = ids(dag, sortedWalk(dag, {dag.idxOf(merge)}));
  CHECK(naive[0] == root);
}

TEST(merge_base_diamond) {
  Builder b;
  Id base = b.commit("base", 1, {});
  Id l = b.commit("left", 2, {base});
  Id r = b.commit("right", 3, {base});
  Id l2 = b.commit("left2", 4, {l});
  Dag dag;
  CHECK_OK(dag.load(b.store, {l2, r}));
  std::vector<int> mb = dag.mergeBases(dag.idxOf(l2), dag.idxOf(r));
  CHECK(mb.size() == 1 && dag.node(mb[0]).id == base);
  Id m = b.commit("merge", 5, {l2, r});
  CHECK_OK(dag.load(b.store, {m}));
  mb = dag.mergeBases(dag.idxOf(m), dag.idxOf(r));
  CHECK(mb.size() == 1 && dag.node(mb[0]).id == r);
}

TEST(merge_base_criss_cross_picks_newest) {
  Builder b;
  Id root = b.commit("root", 1, {});
  Id a1 = b.commit("a1", 10, {root});
  Id b1 = b.commit("b1", 20, {root});
  Id a2 = b.commit("a2", 30, {a1, b1});
  Id b2 = b.commit("b2", 40, {b1, a1});
  Dag dag;
  CHECK_OK(dag.load(b.store, {a2, b2}));
  std::vector<int> mb = dag.mergeBases(dag.idxOf(a2), dag.idxOf(b2));
  CHECK_EQ(mb.size(), 2u);
  std::vector<Id> got;
  for (int x : mb) got.push_back(dag.node(x).id);
  CHECK(std::find(got.begin(), got.end(), a1) != got.end());
  CHECK(std::find(got.begin(), got.end(), b1) != got.end());
  CHECK(dag.node(dag.chooseBase(mb)).id == b1);
}

TEST(merge_base_unrelated_is_empty) {
  Builder b;
  Id x = b.commit("x", 1, {});
  Id y = b.commit("y", 2, {});
  Dag dag;
  CHECK_OK(dag.load(b.store, {x, y}));
  CHECK(dag.mergeBases(dag.idxOf(x), dag.idxOf(y)).empty());
}

TEST(graph_layout_matches_spec_example) {
  Builder b;
  Id init = b.commit("Initial commit", 1, {});
  Id typo = b.commit("Fix typo", 3, {init});
  Id search = b.commit("Add search", 4, {init});
  Id merge = b.commit("Merge", 5, {typo, search});
  Dag dag;
  CHECK_OK(dag.load(b.store, {merge}));
  GraphLayout g(dag);
  std::vector<int> order = dateOrderWalk(dag, {dag.idxOf(merge)});
  std::string out;
  for (int x : order) {
    GraphRow r = g.next(x);
    for (auto& s : r.before) out += s + "\n";
    out += r.prefix + "X\n";
    for (auto& s : r.after) out += s + "\n";
  }
  CHECK_EQ(out, std::string("*   X\n|\\\n| * X\n* | X\n|/\n* X\n"));
}
