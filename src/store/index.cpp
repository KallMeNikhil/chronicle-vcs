#include "store/index.hpp"

#include <vector>

#include "core/sha256.hpp"
#include "core/strutil.hpp"
#include "util/fs.hpp"

namespace chr {

static const char* kHeader = "CHRONICLE-INDEX 1\n";
static const char* kBadIndex = "; try 'chronicle reset --hard HEAD'";

Status Index::load(const std::string& path) {
  entries.clear();
  writeTimeNs = 0;
  fsu::FileStat st = fsu::lstatPath(path);
  if (!st.exists) return Status::Ok();
  writeTimeNs = st.mtimeNs;
  std::string text;
  CHR_TRY(fsu::readFile(path, &text));
  size_t trailer = text.rfind("#SHA256 ");
  if (!startsWith(text, kHeader) || trailer == std::string::npos || text.back() != '\n')
    return Err(std::string("index file is truncated or corrupt") + kBadIndex);
  std::string want = text.substr(trailer + 8, 64);
  if (sha256(text.data(), trailer).hex() != want || trailer + 8 + 64 + 1 != text.size())
    return Err(std::string("index file has a bad checksum") + kBadIndex);
  std::string body = text.substr(std::string(kHeader).size(), trailer - std::string(kHeader).size());
  for (const std::string& line : split(body, '\n')) {
    if (line.empty()) continue;
    std::vector<std::string> f = split(line, '\t');
    IndexEntry e;
    int64_t size = 0;
    std::string p;
    if (f.size() != 5 || !parseMode(f[0], &e.mode) || !Id::fromHex(f[1], &e.id) || !parseInt64(f[2], &size) ||
        !parseInt64(f[3], &e.mtimeNs) || !unescapePath(f[4], &p) || p.empty())
      return Err(std::string("index file has a malformed entry") + kBadIndex);
    e.size = static_cast<uint64_t>(size);
    entries[p] = e;
  }
  return Status::Ok();
}

Status Index::save(const std::string& path) const {
  std::string out = kHeader;
  for (const auto& kv : entries) {
    const IndexEntry& e = kv.second;
    out += modeString(e.mode) + "\t" + e.id.hex() + "\t" + std::to_string(e.size) + "\t" +
           std::to_string(e.mtimeNs) + "\t" + escapePath(kv.first) + "\n";
  }
  out += "#SHA256 " + sha256(out).hex() + "\n";
  return fsu::writeFileAtomic(path, out);
}

using It = std::map<std::string, IndexEntry>::const_iterator;

static Status buildRange(ObjectStore& store, It b, It e, size_t prefixLen, Id* out) {
  std::vector<TreeEntry> items;
  It it = b;
  while (it != e) {
    const std::string& p = it->first;
    size_t slash = p.find('/', prefixLen);
    if (slash == std::string::npos) {
      items.push_back({p.substr(prefixLen), it->second.mode, it->second.id});
      ++it;
      continue;
    }
    std::string dirPrefix = p.substr(0, slash + 1);
    It end = it;
    while (end != e && startsWith(end->first, dirPrefix)) ++end;
    Id sub;
    CHR_TRY(buildRange(store, it, end, dirPrefix.size(), &sub));
    items.push_back({p.substr(prefixLen, slash - prefixLen), kModeDir, sub});
    it = end;
  }
  return store.put(ObjType::Tree, encodeTree(items), out);
}

Status Index::buildTree(ObjectStore& store, Id* root) const {
  return buildRange(store, entries.begin(), entries.end(), 0, root);
}

size_t Index::eraseTree(const std::string& path) {
  size_t n = entries.erase(path);
  std::string pre = path + "/";
  auto it = entries.lower_bound(pre);
  while (it != entries.end() && startsWith(it->first, pre)) {
    it = entries.erase(it);
    ++n;
  }
  return n;
}

bool Index::hasPrefix(const std::string& dir) const {
  std::string pre = dir + "/";
  auto it = entries.lower_bound(pre);
  return it != entries.end() && startsWith(it->first, pre);
}

}
