#include "store/packidx.hpp"

#include "core/sha256.hpp"

namespace chr {

static void putU32(std::string& s, uint32_t v) {
  for (int i = 3; i >= 0; --i) s += static_cast<char>((v >> (8 * i)) & 0xff);
}
static void putU64(std::string& s, uint64_t v) {
  for (int i = 7; i >= 0; --i) s += static_cast<char>((v >> (8 * i)) & 0xff);
}
static uint64_t getBE(const std::string& s, size_t pos, int bytes) {
  uint64_t v = 0;
  for (int i = 0; i < bytes; ++i) v = (v << 8) | static_cast<uint8_t>(s[pos + static_cast<size_t>(i)]);
  return v;
}

std::string encodePackIndex(std::vector<IdEntry>& entries) {
  radixSortIds(entries);
  std::string out = "CIDX";
  putU32(out, 1);
  putU32(out, static_cast<uint32_t>(entries.size()));
  for (const IdEntry& e : entries) {
    out.append(reinterpret_cast<const char*>(e.id.b), 32);
    putU64(out, e.offset);
  }
  Id trailer = sha256(out);
  out.append(reinterpret_cast<const char*>(trailer.b), 32);
  return out;
}

Status decodePackIndex(const std::string& bytes, std::vector<IdEntry>* out) {
  out->clear();
  if (bytes.size() < 12 + 32 || bytes.compare(0, 4, "CIDX") != 0 || getBE(bytes, 4, 4) != 1)
    return CorruptErr("pack index: bad header");
  uint64_t count = getBE(bytes, 8, 4);
  if (bytes.size() != 12 + count * 40 + 32) return CorruptErr("pack index: bad size");
  if (!(sha256(bytes.data(), bytes.size() - 32) == Id::fromRaw(bytes.data() + bytes.size() - 32)))
    return CorruptErr("pack index: checksum mismatch");
  out->resize(static_cast<size_t>(count));
  for (size_t i = 0; i < count; ++i) {
    size_t p = 12 + i * 40;
    (*out)[i].id = Id::fromRaw(bytes.data() + p);
    (*out)[i].offset = getBE(bytes, p + 32, 8);
    if (i > 0 && !((*out)[i - 1].id < (*out)[i].id)) return CorruptErr("pack index: ids not sorted");
  }
  return Status::Ok();
}

}
