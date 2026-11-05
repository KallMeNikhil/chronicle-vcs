#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "ds/radixsort.hpp"
#include "store/object.hpp"

namespace chr {

enum PackKind : uint8_t { kPackBlob = 1, kPackTree = 2, kPackCommit = 3, kPackDelta = 4 };

class Pack {
 public:
  ~Pack();
  Status open(const std::string& packPath, const std::string& idxPath);
  bool find(const Id& id, uint64_t* offset) const;
  Status readEntry(uint64_t offset, uint8_t* kind, std::string* data, Id* base) const;
  Status entryInfo(uint64_t offset, uint8_t* kind, uint64_t* fullSize) const;
  Status verifyTrailer() const;
  const std::vector<IdEntry>& index() const { return index_; }
  const std::string& packPath() const { return packPath_; }
  const std::string& idxPath() const { return idxPath_; }
  uint64_t fileSize() const { return size_; }

 private:
  Status readAt(uint64_t off, size_t len, std::string* out) const;
  std::string packPath_, idxPath_;
  std::vector<IdEntry> index_;
  int fd_ = -1;
  uint64_t size_ = 0;
};

struct PackInput {
  Id id;
  ObjType type;
  std::string payload;
  uint32_t nameHint = 0;
};

struct PackParams {
  int window = 10;
  int maxDepth = 50;
  int minSaving = 50;
};

struct PackWriteStats {
  size_t objects = 0;
  size_t deltas = 0;
  uint64_t fullBytes = 0;
  uint64_t packBytes = 0;
  std::string name;
};

Status writePack(const std::string& packDir, std::vector<PackInput>& objs, const PackParams& params,
                 PackWriteStats* stats);

}
