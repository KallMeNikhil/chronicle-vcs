#pragma once
#include <string>

#include "store/config.hpp"
#include "store/objstore.hpp"
#include "store/refs.hpp"

namespace chr {

class Repo {
 public:
  std::string work;
  std::string dir;
  Config config;
  ObjectStore store;
  Refs refs;

  Status open(const std::string& start = ".");
  static Status init(const std::string& path, std::string* createdDir);

  std::string path(const std::string& rel) const { return dir + "/" + rel; }
  std::string authorIdent() const;
  int64_t maxFileBytes() const { return config.getInt("core.maxfilebytes", 67108864); }
  int abbrev() const { return static_cast<int>(config.getInt("core.abbrev", 8)); }
};

}
