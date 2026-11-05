#include "store/config.hpp"

#include "core/strutil.hpp"
#include "util/fs.hpp"

namespace chr {

Status Config::load(const std::string& path) {
  values_.clear();
  if (!fsu::exists(path)) return Status::Ok();
  std::string text;
  CHR_TRY(fsu::readFile(path, &text));
  int lineNo = 0;
  for (const std::string& raw : split(text, '\n')) {
    ++lineNo;
    std::string line = trim(raw);
    if (line.empty() || line[0] == '#') continue;
    size_t eq = line.find('=');
    if (eq == std::string::npos) return Err("config line " + std::to_string(lineNo) + ": expected key=value");
    values_[trim(line.substr(0, eq))] = trim(line.substr(eq + 1));
  }
  return Status::Ok();
}

std::string Config::get(const std::string& key, const std::string& def) const {
  auto it = values_.find(key);
  return it == values_.end() ? def : it->second;
}

int64_t Config::getInt(const std::string& key, int64_t def) const {
  int64_t v = 0;
  auto it = values_.find(key);
  if (it == values_.end() || !parseInt64(it->second, &v)) return def;
  return v;
}

}
