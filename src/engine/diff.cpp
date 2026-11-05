#include "engine/diff.hpp"

#include <algorithm>
#include <cstdio>
#include <unordered_map>

namespace chr {

std::vector<std::string> splitLines(const std::string& text) {
  std::vector<std::string> out;
  size_t start = 0;
  while (start < text.size()) {
    size_t nl = text.find('\n', start);
    if (nl == std::string::npos) {
      out.push_back(text.substr(start));
      break;
    }
    out.push_back(text.substr(start, nl - start + 1));
    start = nl + 1;
  }
  return out;
}

std::string joinLines(const std::vector<std::string>& lines) {
  std::string s;
  for (const std::string& l : lines) s += l;
  return s;
}

bool isBinary(const std::string& data) {
  return data.find('\0') < 8000 && data.find('\0') != std::string::npos;
}

static const size_t kTraceBudget = 32u << 20;

static std::vector<DiffOp> replaceAll(int n, int m, int offA, int offB) {
  std::vector<DiffOp> ops;
  for (int i = 0; i < n; ++i) ops.push_back({'-', offA + i, -1});
  for (int j = 0; j < m; ++j) ops.push_back({'+', -1, offB + j});
  return ops;
}

static std::vector<DiffOp> myers(const int* a, int n, const int* b, int m, int offA, int offB, int maxD) {
  if (n == 0 || m == 0) return replaceAll(n, m, offA, offB);
  const int max = n + m;
  const int off = max + 1;
  std::vector<int> v(static_cast<size_t>(2 * max + 3), 0);
  std::vector<std::vector<int>> trace;
  size_t traceSize = 0;
  int found = -1;
  for (int d = 0; d <= max && d <= maxD; ++d) {
    for (int k = -d; k <= d; k += 2) {
      int x;
      if (k == -d || (k != d && v[off + k - 1] < v[off + k + 1]))
        x = v[off + k + 1];
      else
        x = v[off + k - 1] + 1;
      int y = x - k;
      while (x < n && y < m && a[x] == b[y]) ++x, ++y;
      v[off + k] = x;
      if (x >= n && y >= m) {
        found = d;
        break;
      }
    }
    if (found >= 0) break;
    traceSize += static_cast<size_t>(2 * d + 1);
    if (traceSize > kTraceBudget) break;
    trace.emplace_back(v.begin() + off - d, v.begin() + off + d + 1);
  }
  if (found < 0) return replaceAll(n, m, offA, offB);

  std::vector<DiffOp> rev;
  int x = n, y = m;
  for (int d = found; d > 0; --d) {
    const std::vector<int>& pv = trace[static_cast<size_t>(d - 1)];
    auto P = [&](int k) { return pv[static_cast<size_t>(k + d - 1)]; };
    int k = x - y;
    int prevK = (k == -d || (k != d && P(k - 1) < P(k + 1))) ? k + 1 : k - 1;
    int prevX = P(prevK);
    int prevY = prevX - prevK;
    int startX = prevK == k + 1 ? prevX : prevX + 1;
    while (x > startX) {
      --x, --y;
      rev.push_back({'=', offA + x, offB + y});
    }
    if (prevK == k + 1)
      rev.push_back({'+', -1, offB + prevY});
    else
      rev.push_back({'-', offA + prevX, -1});
    x = prevX;
    y = prevY;
  }
  while (x > 0) {
    --x, --y;
    rev.push_back({'=', offA + x, offB + y});
  }
  std::reverse(rev.begin(), rev.end());
  return rev;
}

std::vector<DiffOp> diffSeq(const std::vector<int>& a, const std::vector<int>& b, int maxD) {
  int n = static_cast<int>(a.size()), m = static_cast<int>(b.size());
  int pre = 0;
  while (pre < n && pre < m && a[pre] == b[pre]) ++pre;
  int suf = 0;
  while (suf < n - pre && suf < m - pre && a[n - 1 - suf] == b[m - 1 - suf]) ++suf;
  std::vector<DiffOp> ops;
  for (int i = 0; i < pre; ++i) ops.push_back({'=', i, i});
  std::vector<DiffOp> mid = myers(a.data() + pre, n - pre - suf, b.data() + pre, m - pre - suf, pre, pre, maxD);
  ops.insert(ops.end(), mid.begin(), mid.end());
  for (int i = 0; i < suf; ++i) ops.push_back({'=', n - suf + i, m - suf + i});
  return ops;
}

std::vector<DiffOp> diffLines(const std::vector<std::string>& a, const std::vector<std::string>& b) {
  std::unordered_map<std::string, int> intern;
  auto idsOf = [&](const std::vector<std::string>& lines) {
    std::vector<int> ids;
    ids.reserve(lines.size());
    for (const std::string& l : lines) ids.push_back(intern.emplace(l, static_cast<int>(intern.size())).first->second);
    return ids;
  };
  std::vector<int> ia = idsOf(a), ib = idsOf(b);
  return diffSeq(ia, ib);
}

std::vector<Region> regionsFromOps(const std::vector<DiffOp>& ops, const std::vector<std::string>& b) {
  std::vector<Region> out;
  int apos = 0;
  size_t i = 0;
  while (i < ops.size()) {
    if (ops[i].kind == '=') {
      apos = ops[i].a + 1;
      ++i;
      continue;
    }
    Region r{apos, apos, {}};
    while (i < ops.size() && ops[i].kind != '=') {
      if (ops[i].kind == '-') r.be = ops[i].a + 1;
      else r.lines.push_back(b[static_cast<size_t>(ops[i].b)]);
      ++i;
    }
    apos = r.be;
    out.push_back(r);
  }
  return out;
}

std::vector<std::string> applyRegions(const std::vector<std::string>& a, const std::vector<Region>& regions) {
  std::vector<std::string> out;
  int pos = 0;
  for (const Region& r : regions) {
    for (; pos < r.bs; ++pos) out.push_back(a[static_cast<size_t>(pos)]);
    out.insert(out.end(), r.lines.begin(), r.lines.end());
    pos = r.be;
  }
  for (; pos < static_cast<int>(a.size()); ++pos) out.push_back(a[static_cast<size_t>(pos)]);
  return out;
}

static void emitLine(std::string& out, char tag, const std::string& line) {
  out += tag;
  out += line;
  if (line.empty() || line.back() != '\n') out += "\n\\ No newline at end of file\n";
}

std::string unifiedHunks(const std::string& at, const std::string& bt, int context) {
  std::vector<std::string> a = splitLines(at), b = splitLines(bt);
  std::vector<DiffOp> ops = diffLines(a, b);
  std::string out;
  size_t i = 0;
  const size_t n = ops.size();
  while (i < n) {
    while (i < n && ops[i].kind == '=') ++i;
    if (i >= n) break;
    size_t start = i >= static_cast<size_t>(context) ? i - static_cast<size_t>(context) : 0;
    size_t end = i;
    for (;;) {
      while (end < n && ops[end].kind != '=') ++end;
      size_t eq = end;
      while (eq < n && ops[eq].kind == '=') ++eq;
      if (eq < n && eq - end <= static_cast<size_t>(2 * context)) {
        end = eq;
        continue;
      }
      end = std::min(n, end + static_cast<size_t>(context));
      break;
    }
    int aStart = -1, bStart = -1, aLen = 0, bLen = 0;
    int ap = 0, bp = 0;
    for (size_t k = 0; k < start; ++k) {
      if (ops[k].kind != '+') ++ap;
      if (ops[k].kind != '-') ++bp;
    }
    std::string body;
    for (size_t k = start; k < end; ++k) {
      const DiffOp& o = ops[k];
      if (o.kind == '=') {
        emitLine(body, ' ', a[static_cast<size_t>(o.a)]);
        ++aLen, ++bLen;
      } else if (o.kind == '-') {
        emitLine(body, '-', a[static_cast<size_t>(o.a)]);
        ++aLen;
      } else {
        emitLine(body, '+', b[static_cast<size_t>(o.b)]);
        ++bLen;
      }
    }
    aStart = aLen ? ap + 1 : ap;
    bStart = bLen ? bp + 1 : bp;
    char hdr[96];
    std::snprintf(hdr, sizeof hdr, "@@ -%d,%d +%d,%d @@\n", aStart, aLen, bStart, bLen);
    out += hdr;
    out += body;
    i = end;
  }
  return out;
}

}
