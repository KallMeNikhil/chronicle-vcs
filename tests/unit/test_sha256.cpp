#include <cstdio>
#include <cstdlib>
#include <random>

#include "core/sha256.hpp"
#include "framework.hpp"

using namespace chr;

TEST(sha256_nist_vectors) {
  CHECK_EQ(sha256("").hex(), std::string("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
  CHECK_EQ(sha256("abc").hex(), std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
  CHECK_EQ(sha256(std::string(1000000, 'a')).hex(),
           std::string("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
  CHECK_EQ(sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq").hex(),
           std::string("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
}

TEST(sha256_streaming_matches_oneshot) {
  std::mt19937 rng(7);
  std::string data(5000, '\0');
  for (char& c : data) c = static_cast<char>(rng());
  for (size_t chunk : {1u, 3u, 63u, 64u, 65u, 1000u}) {
    Sha256 h;
    for (size_t i = 0; i < data.size(); i += chunk) h.update(data.substr(i, chunk));
    CHECK_EQ(h.finish().hex(), sha256(data).hex());
  }
}

static std::string sha256sumOf(const std::string& data) {
  char path[] = "/tmp/chr-sha-XXXXXX";
  int fd = mkstemp(path);
  if (fd < 0) return "";
  FILE* f = fdopen(fd, "wb");
  std::fwrite(data.data(), 1, data.size(), f);
  std::fclose(f);
  std::string cmd = std::string("sha256sum ") + path + " 2>/dev/null";
  FILE* p = popen(cmd.c_str(), "r");
  std::string out;
  if (p) {
    char buf[128];
    if (std::fgets(buf, sizeof buf, p)) out = std::string(buf).substr(0, 64);
    pclose(p);
  }
  std::remove(path);
  return out;
}

TEST(sha256_block_boundaries_vs_sha256sum) {
  if (std::system("command -v sha256sum >/dev/null 2>&1") != 0) {
    std::printf("  (sha256sum missing: oracle skipped)\n");
    return;
  }
  for (size_t len : {0u, 1u, 55u, 56u, 63u, 64u, 65u, 119u, 120u, 128u, 1000u}) {
    std::string data(len, '\0');
    for (size_t i = 0; i < len; ++i) data[i] = static_cast<char>('a' + i % 26);
    CHECK_EQ(sha256(data).hex(), sha256sumOf(data));
  }
}

TEST(id_hex_round_trip) {
  Id id = sha256("x");
  Id back;
  CHECK(Id::fromHex(id.hex(), &back));
  CHECK(back == id);
  CHECK(!Id::fromHex("zz", &back));
  CHECK(!Id::fromHex(std::string(64, 'g'), &back));
  CHECK(Id().isZero());
  CHECK(!id.isZero());
}
