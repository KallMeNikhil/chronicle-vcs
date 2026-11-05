#include <map>

#include "engine/blame.hpp"
#include "engine/commitops.hpp"
#include "engine/history.hpp"
#include "framework.hpp"
#include "store/index.hpp"
#include "util/fs.hpp"

using namespace chr;

std::string makeTempDir();

namespace {
struct Hist {
  std::string dir = makeTempDir();
  Repo repo;
  int n = 0;
  Hist() {
    Repo::init(dir, nullptr);
    repo.open(dir);
    setenv("CHRONICLE_AUTHOR_DATE", "1700000000 +0000", 1);
  }
  ~Hist() {
    unsetenv("CHRONICLE_AUTHOR_DATE");
    fsu::removeTree(dir);
  }
  Id commit(const std::map<std::string, std::string>& files, std::vector<Id> parents) {
    Index idx;
    for (const auto& kv : files) {
      IndexEntry e;
      repo.store.put(ObjType::Blob, kv.second, &e.id);
      idx.entries[kv.first] = e;
    }
    Id tree, id;
    idx.buildTree(repo.store, &tree);
    createCommit(repo, tree, parents, "c" + std::to_string(++n), &id);
    repo.refs.writeBranch("b" + std::to_string(n), id);
    appendHistory(repo, id);
    return id;
  }
};
}

TEST(history_events_relative_to_first_parent) {
  Hist h;
  Id c1 = h.commit({{"f", "1\n"}, {"g", "g\n"}}, {});
  Id c2 = h.commit({{"f", "2\n"}, {"h", "h\n"}}, {c1});
  std::vector<HistEvent> ev;
  CHECK_OK(commitEvents(h.repo.store, c2, &ev));
  CHECK_EQ(ev.size(), 3u);
  HistoryIndex hist;
  CHECK_OK(hist.load(h.repo));
  CHECK(hist.forPath("f") && hist.forPath("f")->size() == 2);
  CHECK(hist.forPath("g") && hist.events()[static_cast<size_t>(hist.forPath("g")->back())].kind == 'D');
  Id blob = objectId(ObjType::Blob, "h\n");
  CHECK(hist.forBlob(blob) && hist.forBlob(blob)->size() == 1);
  size_t count = 0;
  CHECK_OK(reindexHistory(h.repo, &count));
  CHECK_EQ(count, 5u);
}

TEST(blame_with_merge_known_attribution) {
  Hist h;
  Id c1 = h.commit({{"f", "a\nb\nc\n"}}, {});
  Id c2 = h.commit({{"f", "a\nB\nc\n"}}, {c1});
  Id c3 = h.commit({{"f", "a\nb\nc\nd\n"}}, {c1});
  Id c4 = h.commit({{"f", "a\nB\nc\nd\n"}}, {c2, c3});
  Id c5 = h.commit({{"f", "a\nB\nc\nd\n"}, {"g", "x\n"}}, {c4});
  Dag dag;
  CHECK_OK(dag.load(h.repo.store, {c5}));
  HistoryIndex hist;
  CHECK_OK(hist.load(h.repo));
  Blamer b(h.repo.store, dag, hist);
  std::vector<int> o;
  std::vector<std::string> lines;
  CHECK_OK(b.blame(c5, "f", &o, &lines));
  CHECK_EQ(lines.size(), 4u);
  std::vector<Id> got;
  for (int x : o) got.push_back(dag.node(x).id);
  CHECK(got == (std::vector<Id>{c1, c2, c1, c3}));
  size_t diffs = b.diffsRun();
  CHECK_OK(b.blame(c2, "f", &o, &lines));
  CHECK(dag.node(o[1]).id == c2 && dag.node(o[0]).id == c1);
  CHECK_EQ(b.diffsRun(), diffs);
  Blamer nm(h.repo.store, dag, hist);
  nm.setMemo(false);
  std::vector<int> o2;
  CHECK_OK(nm.blame(c5, "f", &o2, &lines));
  std::vector<Id> got2;
  for (int x : o2) got2.push_back(dag.node(x).id);
  CHECK(got2 == got);
  CHECK(!b.blame(c5, "nope", &o, &lines).ok());
}

TEST(blame_merge_line_new_in_merge_and_binary) {
  Hist h;
  Id c1 = h.commit({{"f", "x\n"}, {"bin", std::string("a\0b", 3)}}, {});
  Id c2 = h.commit({{"f", "x\nleft\n"}, {"bin", std::string("a\0b", 3)}}, {c1});
  Id c3 = h.commit({{"f", "right\nx\n"}, {"bin", std::string("a\0b", 3)}}, {c1});
  Id m = h.commit({{"f", "right\nx\nleft\nresolved\n"}, {"bin", std::string("a\0b", 3)}}, {c2, c3});
  Dag dag;
  CHECK_OK(dag.load(h.repo.store, {m}));
  HistoryIndex hist;
  CHECK_OK(hist.load(h.repo));
  Blamer b(h.repo.store, dag, hist);
  std::vector<int> o;
  std::vector<std::string> lines;
  CHECK_OK(b.blame(m, "f", &o, &lines));
  CHECK(dag.node(o[0]).id == c3);
  CHECK(dag.node(o[1]).id == c1);
  CHECK(dag.node(o[2]).id == c2);
  CHECK(dag.node(o[3]).id == m);
  Status s = b.blame(m, "bin", &o, &lines);
  CHECK(!s.ok());
  CHECK(s.msg.find("binary") != std::string::npos);
}
