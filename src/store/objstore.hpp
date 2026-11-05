#pragma once
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "store/loose.hpp"
#include "store/object.hpp"

namespace chr {

class Pack;

class ObjectStore {
 public:
  ObjectStore();
  ~ObjectStore();
  ObjectStore(const ObjectStore&) = delete;
  ObjectStore& operator=(const ObjectStore&) = delete;

  void setDir(const std::string& objectsDir);
  const std::string& dir() const { return dir_; }

  Status put(ObjType t, const std::string& payload, Id* out);
  Status get(const Id& id, ObjType* t, std::string* payload);
  bool has(const Id& id);
  Status forEachId(const std::function<Status(const Id&)>& fn);

  Status readBlob(const Id& id, std::string* out);
  Status readTree(const Id& id, std::vector<TreeEntry>* out);
  Status readCommit(const Id& id, Commit* out);

  const LooseStore& loose() const { return loose_; }
  Status packs(std::vector<Pack*>* out);
  void reloadPacks();
  void clearCache() { cache_.clear(); }
  void setMaxDepth(int d) { maxDepth_ = d; }

 private:
  struct Cached {
    ObjType type;
    std::string payload;
  };
  Status getPacked(const Id& id, ObjType* t, std::string* payload, bool* found);
  void remember(const Id& id, ObjType t, const std::string& payload);

  std::string dir_;
  LooseStore loose_;
  bool packsLoaded_ = false;
  int maxDepth_ = 50;
  std::vector<std::unique_ptr<Pack>> packs_;
  std::unordered_map<Id, Cached, IdHash> cache_;
};

}
