#include <random>

#include "core/sha256.hpp"
#include "framework.hpp"
#include "store/delta.hpp"
#include "store/objstore.hpp"
#include "store/pack.hpp"
#include "store/packidx.hpp"
#include "util/fs.hpp"

using namespace chr;

std::string makeTempDir();

static std::string randomBytes(std::mt19937& rng, size_t n) {
  std::string s(n, '\0');
  for (char& c : s) c = static_cast<char>(rng());
  return s;
}

TEST(delta_round_trip_random_and_structured) {
  std::mt19937 rng(77);
  int bad = 0;
  for (int t = 0; t < 400; ++t) {
    std::string base = randomBytes(rng, rng() % 5000);
    std::string target;
    switch (t % 4) {
      case 0: target = randomBytes(rng, rng() % 3000); break;
      case 1: target = base; break;
      case 2: {
        target = base;
        for (int k = 0; k < 5 && !target.empty(); ++k) {
          size_t p = rng() % target.size();
          target.insert(p, randomBytes(rng, rng() % 40));
          if (rng() % 2 && p + 30 < target.size()) target.erase(p, 30);
        }
        break;
      }
      default: target = base.substr(base.size() / 3) + "tail" + base.substr(0, base.size() / 3);
    }
    std::string d = makeDelta(base, target), back;
    if (!applyDelta(base, d, &back).ok() || back != target) ++bad;
    if (deltaResultSize(d) != target.size()) ++bad;
    if (t % 4 == 1 && base.size() > 1000 && d.size() > 40) ++bad;
  }
  CHECK_EQ(bad, 0);
  std::string text;
  for (int i = 0; i < 500; ++i) text += "line number " + std::to_string(i) + " of the file\n";
  std::string edited = text;
  edited.replace(5000, 10, "EDITED!!!!");
  std::string d = makeDelta(text, edited);
  CHECK(d.size() < text.size() / 20);
  std::string back;
  CHECK_OK(applyDelta(text, d, &back));
  CHECK(back == edited);
  CHECK(!applyDelta("abc", std::string("\x05\x01\x00\x09", 4), &back).ok());
}

TEST(pack_index_round_trip) {
  std::vector<IdEntry> v;
  for (int i = 0; i < 1000; ++i) v.push_back({sha256(std::to_string(i)), static_cast<uint64_t>(i * 13)});
  std::string enc = encodePackIndex(v);
  std::vector<IdEntry> back;
  CHECK_OK(decodePackIndex(enc, &back));
  CHECK_EQ(back.size(), 1000u);
  for (size_t i = 1; i < back.size(); ++i) CHECK(back[i - 1].id < back[i].id);
  enc[100] ^= 1;
  CHECK(!decodePackIndex(enc, &back).ok());
}

TEST(pack_round_trip_with_delta_chains) {
  std::string dir = makeTempDir();
  ObjectStore store;
  store.setDir(dir + "/objects");
  std::vector<PackInput> in;
  std::vector<std::pair<Id, std::string>> expect;
  std::string doc;
  for (int i = 0; i < 300; ++i) doc += "paragraph " + std::to_string(i) + " lorem ipsum dolor sit amet\n";
  for (int v = 0; v < 60; ++v) {
    doc += "appended in version " + std::to_string(v) + "\n";
    PackInput p{objectId(ObjType::Blob, doc), ObjType::Blob, doc, 7};
    expect.push_back({p.id, doc});
    in.push_back(p);
  }
  std::mt19937 rng(5);
  for (int i = 0; i < 40; ++i) {
    std::string r = randomBytes(rng, rng() % 2000);
    PackInput p{objectId(ObjType::Blob, r), ObjType::Blob, r, static_cast<uint32_t>(i)};
    expect.push_back({p.id, r});
    in.push_back(p);
  }
  std::string treePayload = encodeTree({{"f", kModeFile, expect[0].first}});
  in.push_back({objectId(ObjType::Tree, treePayload), ObjType::Tree, treePayload, 0});
  PackParams params;
  params.maxDepth = 5;
  PackWriteStats st;
  CHECK_OK(writePack(dir + "/objects/pack", in, params, &st));
  CHECK_EQ(st.objects, 101u);
  CHECK(st.deltas >= 50);
  CHECK(st.packBytes < st.fullBytes / 4);
  ObjectStore fresh;
  fresh.setDir(dir + "/objects");
  int bad = 0;
  for (auto& e : expect) {
    std::string got;
    if (!fresh.has(e.first) || !fresh.readBlob(e.first, &got).ok() || got != e.second) ++bad;
  }
  CHECK_EQ(bad, 0);
  std::vector<TreeEntry> tree;
  CHECK_OK(fresh.readTree(objectId(ObjType::Tree, treePayload), &tree));
  int count = 0;
  CHECK_OK(fresh.forEachId([&](const Id&) {
    ++count;
    return Status::Ok();
  }));
  CHECK_EQ(count, 101);
  std::vector<Pack*> packs;
  CHECK_OK(fresh.packs(&packs));
  CHECK_EQ(packs.size(), 1u);
  CHECK_OK(packs[0]->verifyTrailer());
  fsu::removeTree(dir);
}
