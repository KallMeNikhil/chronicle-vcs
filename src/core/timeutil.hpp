#pragma once
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>

#include "core/strutil.hpp"

namespace chr {

inline int64_t nowSeconds() { return static_cast<int64_t>(std::time(nullptr)); }

inline int localTzOffsetMinutes(int64_t t) {
  time_t tt = static_cast<time_t>(t);
  struct tm lt;
  localtime_r(&tt, &lt);
  return static_cast<int>(lt.tm_gmtoff / 60);
}

inline std::string formatTz(int minutes) {
  char buf[8];
  int a = minutes < 0 ? -minutes : minutes;
  std::snprintf(buf, sizeof buf, "%c%02d%02d", minutes < 0 ? '-' : '+', (a / 60) % 100, a % 60);
  return buf;
}

inline bool parseTz(const std::string& s, int* minutes) {
  if (s.size() != 5 || (s[0] != '+' && s[0] != '-')) return false;
  for (size_t i = 1; i < 5; ++i)
    if (s[i] < '0' || s[i] > '9') return false;
  int v = ((s[1] - '0') * 10 + (s[2] - '0')) * 60 + (s[3] - '0') * 10 + (s[4] - '0');
  *minutes = s[0] == '-' ? -v : v;
  return true;
}

inline struct tm brokenDown(int64_t t, int tzMinutes) {
  time_t shifted = static_cast<time_t>(t + static_cast<int64_t>(tzMinutes) * 60);
  struct tm g;
  gmtime_r(&shifted, &g);
  return g;
}

inline std::string formatDate(int64_t t, int tzMinutes) {
  struct tm g = brokenDown(t, tzMinutes);
  char buf[32];
  std::snprintf(buf, sizeof buf, "%04d-%02d-%02d", g.tm_year + 1900, g.tm_mon + 1, g.tm_mday);
  return buf;
}

inline std::string formatDateTime(int64_t t, int tzMinutes) {
  struct tm g = brokenDown(t, tzMinutes);
  char buf[48];
  std::snprintf(buf, sizeof buf, "%04d-%02d-%02d %02d:%02d:%02d %s", g.tm_year + 1900, g.tm_mon + 1,
                g.tm_mday, g.tm_hour, g.tm_min, g.tm_sec, formatTz(tzMinutes).c_str());
  return buf;
}

inline bool parseTimeSpec(const std::string& s, int64_t* out) {
  bool allDigits = !s.empty();
  for (char c : s)
    if (c < '0' || c > '9') allDigits = false;
  if (allDigits) return parseInt64(s, out);
  int Y = 0, M = 0, D = 0, h = 0, m = 0, sec = 0;
  auto num = [&](size_t pos, size_t len, int* v) {
    if (pos + len > s.size()) return false;
    int r = 0;
    for (size_t i = pos; i < pos + len; ++i) {
      if (s[i] < '0' || s[i] > '9') return false;
      r = r * 10 + (s[i] - '0');
    }
    *v = r;
    return true;
  };
  if (s.size() < 10 || !num(0, 4, &Y) || s[4] != '-' || !num(5, 2, &M) || s[7] != '-' || !num(8, 2, &D))
    return false;
  if (s.size() > 10) {
    if ((s[10] != 'T' && s[10] != ' ') || !num(11, 2, &h) || s.size() < 16 || s[13] != ':' ||
        !num(14, 2, &m))
      return false;
    if (s.size() > 16) {
      if (s.size() != 19 || s[16] != ':' || !num(17, 2, &sec)) return false;
    }
  }
  if (M < 1 || M > 12 || D < 1 || D > 31 || h > 23 || m > 59 || sec > 60) return false;
  struct tm lt = {};
  lt.tm_year = Y - 1900;
  lt.tm_mon = M - 1;
  lt.tm_mday = D;
  lt.tm_hour = h;
  lt.tm_min = m;
  lt.tm_sec = sec;
  lt.tm_isdst = -1;
  time_t t = mktime(&lt);
  if (t == static_cast<time_t>(-1)) return false;
  *out = static_cast<int64_t>(t);
  return true;
}

}
