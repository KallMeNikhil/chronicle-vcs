#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace chr {

inline bool startsWith(const std::string& s, const std::string& p) {
  return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}

inline bool endsWith(const std::string& s, const std::string& p) {
  return s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0;
}

inline std::vector<std::string> split(const std::string& s, char sep) {
  std::vector<std::string> out;
  size_t start = 0;
  for (;;) {
    size_t pos = s.find(sep, start);
    if (pos == std::string::npos) {
      out.push_back(s.substr(start));
      return out;
    }
    out.push_back(s.substr(start, pos - start));
    start = pos + 1;
  }
}

inline std::string trim(const std::string& s) {
  size_t b = 0, e = s.size();
  while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n')) ++b;
  while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' || s[e - 1] == '\n')) --e;
  return s.substr(b, e - b);
}

inline int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

inline bool isLowerHex(const std::string& s) {
  for (char c : s)
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  return !s.empty();
}

inline std::string toHex(const uint8_t* p, size_t n) {
  static const char* d = "0123456789abcdef";
  std::string out(n * 2, '0');
  for (size_t i = 0; i < n; ++i) {
    out[2 * i] = d[p[i] >> 4];
    out[2 * i + 1] = d[p[i] & 15];
  }
  return out;
}

inline bool parseInt64(const std::string& s, int64_t* out) {
  if (s.empty()) return false;
  size_t i = 0;
  bool neg = false;
  if (s[0] == '-' || s[0] == '+') {
    neg = s[0] == '-';
    i = 1;
    if (s.size() == 1) return false;
  }
  uint64_t v = 0;
  for (; i < s.size(); ++i) {
    if (s[i] < '0' || s[i] > '9') return false;
    uint64_t nv = v * 10 + static_cast<uint64_t>(s[i] - '0');
    if (nv / 10 != v) return false;
    v = nv;
  }
  if (v > static_cast<uint64_t>(INT64_MAX)) return false;
  *out = neg ? -static_cast<int64_t>(v) : static_cast<int64_t>(v);
  return true;
}

inline std::string escapePath(const std::string& p) {
  std::string out;
  out.reserve(p.size());
  static const char* d = "0123456789ABCDEF";
  for (unsigned char c : p) {
    if (c == '%' || c < 0x20 || c == 0x7F) {
      out += '%';
      out += d[c >> 4];
      out += d[c & 15];
    } else {
      out += static_cast<char>(c);
    }
  }
  return out;
}

inline bool unescapePath(const std::string& s, std::string* out) {
  out->clear();
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] != '%') {
      *out += s[i];
      continue;
    }
    if (i + 2 >= s.size()) return false;
    int h = hexVal(s[i + 1]), l = hexVal(s[i + 2]);
    if (h < 0 || l < 0) return false;
    *out += static_cast<char>(h * 16 + l);
    i += 2;
  }
  return true;
}

inline std::string displayPath(const std::string& p) {
  for (unsigned char c : p)
    if (c < 0x20 || c == 0x7F) return "\"" + escapePath(p) + "\"";
  return p;
}

}
