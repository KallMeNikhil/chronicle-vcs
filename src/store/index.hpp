#pragma once
#include <cstdint>
#include <map>
#include <string>

#include "store/object.hpp"
#include "store/objstore.hpp"

namespace chr {

struct IndexEntry {
  uint32_t mode = kModeFile;
  Id id;
  uint64_t size = 0;
  int64_t mtimeNs = 0;
};

class Index {
 public:
  std::map<std::string, IndexEntry> entries;
  int64_t writeTimeNs = 0;

  Status load(const std::string& path);
  Status save(const std::string& path) const;
  Status buildTree(ObjectStore& store, Id* root) const;
  size_t eraseTree(const std::string& path);
  bool hasPrefix(const std::string& dir) const;
};

}
