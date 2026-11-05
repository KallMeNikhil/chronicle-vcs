#include <cstdlib>
#include <random>

#include "bench_util.hpp"
#include "engine/blame.hpp"
#include "engine/commitops.hpp"
#include "engine/diff.hpp"
#include "engine/gc.hpp"
#include "engine/history.hpp"
#include "store/index.hpp"
#include "util/fs.hpp"

using namespace chr;

namespace bench {

namespace {
Status buildHistory(const std::string& dir, size_t n, size_t lines, Repo* repo, std::vector<Id>* commits) {
  CHR_TRY(Repo::init(dir, nullptr));
  CHR_TRY(repo->open(dir));
  std::mt19937 rng(42);
  std::vector<std::string> file;
  for (size_t i = 0; i < lines; ++i) file.push_back("int value_" + std::to_string(i) + " = compute(" + std::to_string(i) + ");\n");
  std::string other = "notes\n";
  Id parent;
  for (size_t c = 0; c < n; ++c) {
    size_t at = rng() % file.size();
    if (rng() % 3 == 0) file.insert(file.begin() + static_cast<long>(at), "int added_in_commit_" + std::to_string(c) + " = 0;\n");
    else file[at] = "int value_" + std::to_string(at) + " = revised(" + std::to_string(c) + ");\n";
    if (c % 5 == 0) other += "note " + std::to_string(c) + "\n";
    Index idx;
    IndexEntry e;
    CHR_TRY(repo->store.put(ObjType::Blob, joinLines(file), &e.id));
    idx.entries["src/main.c"] = e;
    CHR_TRY(repo->store.put(ObjType::Blob, other, &e.id));
    idx.entries["NOTES"] = e;
    Id tree, id;
    CHR_TRY(idx.buildTree(repo->store, &tree));
    setenv("CHRONICLE_AUTHOR_DATE", (std::to_string(1700000000 + c * 60) + " +0000").c_str(), 1);
    std::vector<Id> parents;
    if (c) parents.push_back(parent);
    CHR_TRY(createCommit(*repo, tree, parents, "commit " + std::to_string(c), &id));
    CHR_TRY(recordCommit(*repo, id));
    commits->push_back(id);
    parent = id;
  }
  unsetenv("CHRONICLE_AUTHOR_DATE");
  return Status::Ok();
}
}

static void b5(const std::string& tmp) {
  Series& memo = add("B5", "blame at all N revisions: with memo", "1 (each commit diffed once)");
  Series& naive = add("B5", "blame at all N revisions: no memo", "2 (O(N) diffs per query)");
  for (size_t n : {25u, 50u, 100u, 200u, 400u}) {
    Repo repo;
    std::vector<Id> commits;
    std::string dir = tmp + "/b5_" + std::to_string(n);
    if (!buildHistory(dir, n, 200, &repo, &commits).ok()) return;
    Dag dag;
    dag.load(repo.store, {commits.back()});
    HistoryIndex hist;
    hist.load(repo);
    auto run = [&](bool useMemo) {
      Blamer b(repo.store, dag, hist);
      b.setMemo(useMemo);
      std::vector<int> o;
      std::vector<std::string> l;
      for (size_t i = commits.size(); i-- > 0;) b.blame(commits[i], "src/main.c", &o, &l);
      keep(o.size());
    };
    memo.pts.push_back({double(n), timeIt([&] { run(true); })});
    naive.pts.push_back({double(n), timeIt([&] { run(false); }, 1)});
    fsu::removeTree(dir);
  }
}

static std::vector<std::string> randomFile(size_t n, std::mt19937& rng) {
  std::vector<std::string> v;
  for (size_t i = 0; i < n; ++i) v.push_back("line " + std::to_string(rng()) + "\n");
  return v;
}

static std::vector<std::string> edit(std::vector<std::string> v, size_t d, std::mt19937& rng) {
  for (size_t k = 0; k < d; ++k) v[rng() % v.size()] = "edited " + std::to_string(rng()) + "\n";
  return v;
}

static void b7() {
  Series& byN = add("B7", "diffLines vs file size N (D = 20)", "1 (O(N D), D fixed)");
  Series& byD = add("B7", "Myers core vs edit distance D (N = 2000)", "1..2 (O(N D) worst, ~O(N + D^2))");
  std::mt19937 rng(5);
  for (size_t n = 1000; n <= 128000; n *= 2) {
    std::vector<std::string> a = randomFile(n, rng), b = edit(a, 10, rng);
    byN.pts.push_back({double(n), timeIt([&] { keep(diffLines(a, b).size()); })});
  }
  const int n = 2000;
  std::vector<int> base(n);
  for (int i = 0; i < n; ++i) base[static_cast<size_t>(i)] = i;
  for (int d = 32; d <= 2048; d *= 2) {
    std::vector<int> b = base;
    for (int k = 0; k < d / 2; ++k) b[static_cast<size_t>((k * 2 * n / d) % n)] = 100000 + k;
    byD.pts.push_back({double(d), timeIt([&] { keep(diffSeq(base, b).size()); })});
  }
}

static void b9(const std::string& tmp) {
  Series& loose = add("B9", "storage bytes: loose objects", "1 (full copy per version)", "bytes");
  Series& packed = add("B9", "storage bytes: delta-compressed pack", "1 (small delta per version)", "bytes");
  for (size_t n : {50u, 100u, 200u, 400u, 800u}) {
    Repo repo;
    std::vector<Id> commits;
    std::string dir = tmp + "/b9_" + std::to_string(n);
    if (!buildHistory(dir, n, 400, &repo, &commits).ok()) return;
    std::vector<Id> ids;
    repo.store.loose().list(&ids);
    uint64_t bytes = 0;
    for (const Id& id : ids) bytes += fsu::lstatPath(repo.store.loose().pathFor(id)).size;
    PackWriteStats st;
    packLoose(repo, false, &st);
    loose.pts.push_back({double(n), double(bytes)});
    packed.pts.push_back({double(n), double(st.packBytes)});
    std::fprintf(stderr, "  B9 n=%zu: loose %llu bytes, packed %llu bytes (%.1f%%), %zu/%zu deltas\n", n,
                 static_cast<unsigned long long>(bytes), static_cast<unsigned long long>(st.packBytes),
                 100.0 * double(st.packBytes) / double(bytes), st.deltas, st.objects);
    fsu::removeTree(dir);
  }
}

void benchRepo(const std::string& tmp) {
  std::fprintf(stderr, "B5...\n");
  b5(tmp);
  std::fprintf(stderr, "B7...\n");
  b7();
  std::fprintf(stderr, "B9...\n");
  b9(tmp);
}

}
