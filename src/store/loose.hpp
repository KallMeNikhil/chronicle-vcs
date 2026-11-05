#pragma once
#include <string>
#include <vector>

#include "store/object.hpp"

namespace chr {

class LooseStore {
 public:
  void setDir(const std::string& objectsDir) { dir_ = objectsDir; }
  std::string pathFor(const Id& id) const;
  bool has(const Id& id) const;
  Status read(const Id& id, ObjType* t, std::string* payload) const;
  Status write(ObjType t, const std::string& payload, Id* out) const;
  Status list(std::vector<Id>* out) const;
  Status remove(const Id& id) const;
  int64_t mtimeSeconds(const Id& id) const;

 private:
  std::string dir_;
};

}
