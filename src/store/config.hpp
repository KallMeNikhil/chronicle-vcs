#pragma once
#include <cstdint>
#include <map>
#include <string>

#include "core/status.hpp"

namespace chr {

class Config {
 public:
  Status load(const std::string& path);
  std::string get(const std::string& key, const std::string& def = "") const;
  int64_t getInt(const std::string& key, int64_t def) const;
  void set(const std::string& key, const std::string& value) { values_[key] = value; }

 private:
  std::map<std::string, std::string> values_;
};

}
