#include "store/delta.hpp"

#include <cstring>
#include <unordered_map>
#include <vector>

#include "core/varint.hpp"

namespace chr {

namespace {

const size_t kBlock = 16;
const uint64_t kBase = 257;
const size_t kMaxCandidates = 16;

uint64_t hashBlock(const unsigned char* p) {
  uint64_t h = 0;
  for (size_t i = 0; i < kBlock; ++i) h = h * kBase + p[i];
  return h;
}

uint64_t topPower() {
  uint64_t p = 1;
  for (size_t i = 1; i < kBlock; ++i) p *= kBase;
  return p;
}

void flushInsert(std::string& out, std::string& pending) {
  size_t pos = 0;
  while (pos < pending.size()) {
    size_t len = pending.size() - pos;
    out += '\x02';
    putVarint(out, len);
    out.append(pending, pos, len);
    pos += len;
  }
  pending.clear();
}

}

std::string makeDelta(const std::string& baseStr, const std::string& targetStr) {
  const unsigned char* base = reinterpret_cast<const unsigned char*>(baseStr.data());
  const unsigned char* tgt = reinterpret_cast<const unsigned char*>(targetStr.data());
  const size_t n = baseStr.size(), m = targetStr.size();
  std::string out;
  putVarint(out, m);
  std::unordered_map<uint64_t, std::vector<uint32_t>> index;
  for (size_t off = 0; off + kBlock <= n; off += kBlock) {
    std::vector<uint32_t>& v = index[hashBlock(base + off)];
    if (v.size() < kMaxCandidates) v.push_back(static_cast<uint32_t>(off));
  }
  const uint64_t pw = topPower();
  std::string pending;
  size_t i = 0;
  uint64_t h = 0;
  bool hashValid = false;
  while (i + kBlock <= m) {
    if (!hashValid) {
      h = hashBlock(tgt + i);
      hashValid = true;
    }
    size_t bestLen = 0, bestOff = 0, bestBack = 0;
    auto it = index.find(h);
    if (it != index.end()) {
      for (uint32_t off : it->second) {
        if (std::memcmp(base + off, tgt + i, kBlock) != 0) continue;
        size_t len = kBlock;
        while (off + len < n && i + len < m && base[off + len] == tgt[i + len]) ++len;
        size_t back = 0;
        while (back < pending.size() && back < off && base[off - back - 1] == tgt[i - back - 1]) ++back;
        if (len + back > bestLen + bestBack) {
          bestLen = len;
          bestOff = off;
          bestBack = back;
        }
      }
    }
    if (bestLen >= kBlock) {
      pending.resize(pending.size() - bestBack);
      flushInsert(out, pending);
      out += '\x01';
      putVarint(out, bestOff - bestBack);
      putVarint(out, bestLen + bestBack);
      i += bestLen;
      hashValid = false;
      continue;
    }
    pending += static_cast<char>(tgt[i]);
    if (i + kBlock < m) h = (h - tgt[i] * pw) * kBase + tgt[i + kBlock];
    ++i;
  }
  pending.append(targetStr, i, std::string::npos);
  flushInsert(out, pending);
  return out;
}

uint64_t deltaResultSize(const std::string& delta) {
  size_t pos = 0;
  uint64_t v = 0;
  return getVarint(delta, pos, &v) ? v : 0;
}

Status applyDelta(const std::string& base, const std::string& delta, std::string* out) {
  size_t pos = 0;
  uint64_t size = 0;
  if (!getVarint(delta, pos, &size)) return CorruptErr("delta: bad header");
  out->clear();
  out->reserve(static_cast<size_t>(size));
  while (pos < delta.size()) {
    char op = delta[pos++];
    uint64_t a = 0, b = 0;
    if (op == '\x01') {
      if (!getVarint(delta, pos, &a) || !getVarint(delta, pos, &b) || a > base.size() || b > base.size() - a)
        return CorruptErr("delta: bad COPY");
      out->append(base, static_cast<size_t>(a), static_cast<size_t>(b));
    } else if (op == '\x02') {
      if (!getVarint(delta, pos, &a) || a > delta.size() - pos) return CorruptErr("delta: bad INSERT");
      out->append(delta, pos, static_cast<size_t>(a));
      pos += static_cast<size_t>(a);
    } else {
      return CorruptErr("delta: unknown instruction");
    }
  }
  if (out->size() != size) return CorruptErr("delta: result size mismatch");
  return Status::Ok();
}

}
