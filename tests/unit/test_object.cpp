#include <cstdlib>
#include <random>

#include "core/sha256.hpp"
#include "framework.hpp"
#include "store/objstore.hpp"
#include "store/repo.hpp"
#include "util/fs.hpp"

using namespace chr;

std::string makeTempDir() {
  char tmpl[] = "/tmp/chr-unit-XXXXXX";
  char* d = mkdtemp(tmpl);
  return d ? d : "/tmp";
}

TEST(object_header_and_id) {
  CHECK_EQ(objectHeader(ObjType::Blob, 5), std::string("blob 5\0", 7));
  CHECK_EQ(objectId(ObjType::Blob, "hello\n").hex(), sha256(std::string("blob 6\0hello\n", 13)).hex());
  ObjType t;
  std::string payload;
  CHECK_OK(decodeObject(std::string("tree 3\0abc", 10), &t, &payload));
  CHECK(t == ObjType::Tree);
  CHECK_EQ(payload, std::string("abc"));
  CHECK(!decodeObject(std::string("tree 4\0abc", 10), &t, &payload).ok());
  CHECK(!decodeObject("garbage", &t, &payload).ok());
}

TEST(tree_round_trip_and_order) {
  std::vector<TreeEntry> in = {
      {"zeta", kModeFile, sha256("1")}, {"Alpha", kModeExec, sha256("2")},
      {"a.b", kModeDir, sha256("3")},   {"a", kModeLink, sha256("4")},
      {"with space", kModeFile, sha256("5")}};
  std::string enc = encodeTree(in);
  std::vector<TreeEntry> out;
  CHECK_OK(decodeTree(enc, &out));
  CHECK_EQ(out.size(), 5u);
  CHECK_EQ(out[0].name, std::string("Alpha"));
  CHECK_EQ(out[1].name, std::string("a"));
  CHECK_EQ(out[2].name, std::string("a.b"));
  CHECK_EQ(out[4].name, std::string("zeta"));
  CHECK(out[2].mode == kModeDir);
  CHECK(out[1].id == sha256("4"));
  CHECK_EQ(encodeTree(out), enc);
  CHECK_EQ(decodeTree("", &out).ok(), true);
  CHECK(out.empty());
  CHECK(!decodeTree(enc.substr(0, enc.size() - 3), &out).ok());
}

TEST(entry_name_validation) {
  CHECK(validEntryName("ok.txt"));
  CHECK(!validEntryName(""));
  CHECK(!validEntryName("."));
  CHECK(!validEntryName(".."));
  CHECK(!validEntryName(".chronicle"));
  CHECK(!validEntryName("a/b"));
  CHECK(!validEntryName(std::string("a\0b", 3)));
}

TEST(commit_round_trip) {
  Commit c;
  c.tree = sha256("tree");
  c.parents = {sha256("p1"), sha256("p2")};
  c.author = "Ada Lovelace <ada@example.com>";
  c.time = 1700000000;
  c.tz = 330;
  c.message = "Subject line\n\nBody";
  std::string enc = encodeCommit(c);
  CHECK(enc.find("author Ada Lovelace <ada@example.com> 1700000000 +0530\n") != std::string::npos);
  CHECK(enc.back() == '\n');
  Commit d;
  CHECK_OK(decodeCommit(enc, &d));
  CHECK(d.tree == c.tree);
  CHECK_EQ(d.parents.size(), 2u);
  CHECK(d.parents[1] == c.parents[1]);
  CHECK_EQ(d.author, c.author);
  CHECK_EQ(d.time, c.time);
  CHECK_EQ(d.tz, 330);
  CHECK_EQ(d.message, std::string("Subject line\n\nBody\n"));
  CHECK_EQ(d.summary(), std::string("Subject line"));
  CHECK_EQ(d.authorName(), std::string("Ada Lovelace"));
  CHECK_EQ(encodeCommit(d), enc);
  CHECK(!decodeCommit("tree zz\n\nmsg\n", &d).ok());
}

TEST(loose_store_round_trip_and_corruption) {
  std::string dir = makeTempDir();
  ObjectStore store;
  store.setDir(dir + "/objects");
  std::mt19937 rng(3);
  std::vector<std::pair<Id, std::string>> written;
  for (int i = 0; i < 50; ++i) {
    std::string data(static_cast<size_t>(rng() % 3000), '\0');
    for (char& ch : data) ch = static_cast<char>(rng());
    Id id;
    CHECK_OK(store.put(ObjType::Blob, data, &id));
    CHECK_OK(store.put(ObjType::Blob, data, &id));
    written.push_back({id, data});
  }
  ObjectStore fresh;
  fresh.setDir(dir + "/objects");
  for (auto& w : written) {
    std::string back;
    CHECK_OK(fresh.readBlob(w.first, &back));
    CHECK(back == w.second);
  }
  int count = 0;
  CHECK_OK(fresh.forEachId([&](const Id&) {
    ++count;
    return Status::Ok();
  }));
  CHECK(count >= 1 && count <= 50);
  std::string path = fresh.loose().pathFor(written[0].first);
  CHECK_OK(fsu::removeFile(path));
  CHECK_OK(fsu::writeFile(path, std::string("blob 1\0X", 8)));
  ObjectStore again;
  again.setDir(dir + "/objects");
  std::string back;
  Status s = again.readBlob(written[0].first, &back);
  CHECK_EQ(s.code, kErrCorrupt);
  Id missing = sha256("never stored");
  CHECK_EQ(again.readBlob(missing, &back).code, kErrCorrupt);
  fsu::removeTree(dir);
}

TEST(repo_init_and_open) {
  std::string dir = makeTempDir();
  std::string created;
  CHECK_OK(Repo::init(dir + "/r", &created));
  CHECK(!Repo::init(dir + "/r", &created).ok());
  CHECK_OK(fsu::mkdirs(dir + "/r/sub/deeper"));
  Repo repo;
  CHECK_OK(repo.open(dir + "/r/sub/deeper"));
  CHECK_EQ(repo.work, fsu::absPath(dir + "/r"));
  Repo none;
  CHECK_EQ(none.open(dir).code, kErrRepo);
  fsu::removeTree(dir);
}
