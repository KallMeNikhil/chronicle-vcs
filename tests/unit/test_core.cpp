#include <cstdlib>
#include <random>

#include "core/status.hpp"
#include "core/strutil.hpp"
#include "core/timeutil.hpp"
#include "core/varint.hpp"
#include "framework.hpp"

using namespace chr;

TEST(varint_round_trip) {
  std::mt19937_64 rng(1);
  std::string buf;
  std::vector<uint64_t> vals = {0, 1, 127, 128, 255, 16383, 16384, UINT64_MAX};
  for (int i = 0; i < 1000; ++i) vals.push_back(rng() >> (rng() % 64));
  for (uint64_t v : vals) putVarint(buf, v);
  size_t pos = 0;
  for (uint64_t v : vals) {
    uint64_t got = 0;
    CHECK(getVarint(buf, pos, &got));
    CHECK_EQ(got, v);
  }
  CHECK_EQ(pos, buf.size());
  uint64_t dummy;
  std::string truncated = "\x80";
  size_t p = 0;
  CHECK(!getVarint(truncated, p, &dummy));
}

TEST(path_escape_round_trip) {
  std::string weird = std::string("a b\tc\nd%e\rf\x01g\x7fh") + "\xc3\xa9";
  std::string esc = escapePath(weird);
  CHECK(esc.find('\t') == std::string::npos);
  CHECK(esc.find('\n') == std::string::npos);
  std::string back;
  CHECK(unescapePath(esc, &back));
  CHECK_EQ(back, weird);
  CHECK(!unescapePath("bad%4", &back));
  CHECK(!unescapePath("bad%zz", &back));
}

TEST(int_parsing) {
  int64_t v = 0;
  CHECK(parseInt64("12345", &v));
  CHECK_EQ(v, 12345);
  CHECK(parseInt64("-7", &v));
  CHECK_EQ(v, -7);
  CHECK(!parseInt64("", &v));
  CHECK(!parseInt64("12a", &v));
  CHECK(!parseInt64("99999999999999999999", &v));
}

TEST(split_trim) {
  auto parts = split("a\tb\t\tc", '\t');
  CHECK_EQ(parts.size(), 4u);
  CHECK_EQ(parts[2], std::string(""));
  CHECK_EQ(trim("  x y \n"), std::string("x y"));
}

TEST(time_parsing) {
  setenv("TZ", "UTC", 1);
  tzset();
  int64_t t = 0;
  CHECK(parseTimeSpec("1700000000", &t));
  CHECK_EQ(t, 1700000000);
  CHECK(parseTimeSpec("2023-11-14", &t));
  CHECK_EQ(t, 1699920000);
  CHECK(parseTimeSpec("2023-11-14T22:13", &t));
  CHECK_EQ(t, 1699920000 + 22 * 3600 + 13 * 60);
  CHECK(parseTimeSpec("2023-11-14T22:13:20", &t));
  CHECK_EQ(t, 1700000000);
  CHECK(!parseTimeSpec("2023-13-01", &t));
  CHECK(!parseTimeSpec("yesterday", &t));
  int tz = 0;
  CHECK(parseTz("+0530", &tz));
  CHECK_EQ(tz, 330);
  CHECK_EQ(formatTz(-90), std::string("-0130"));
  CHECK_EQ(formatDate(1700000000, 0), std::string("2023-11-14"));
  CHECK_EQ(formatDate(1700000000, 330), std::string("2023-11-15"));
}

TEST(status_codes) {
  Status s = CorruptErr("x");
  CHECK(!s.ok());
  CHECK_EQ(s.code, 4);
  CHECK(Status::Ok().ok());
}
