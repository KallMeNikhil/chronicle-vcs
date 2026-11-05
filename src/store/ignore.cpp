#include "store/ignore.hpp"

#include <fnmatch.h>

#include "core/strutil.hpp"
#include "util/fs.hpp"

namespace chr {

Status Ignore::load(const std::string& worktreeRoot) {
  rules_.clear();
  std::string path = worktreeRoot + "/.chronicleignore";
  if (!fsu::lstatPath(path).isReg) return Status::Ok();
  std::string text;
  CHR_TRY(fsu::readFile(path, &text));
  for (const std::string& line : split(text, '\n')) addPattern(line);
  return Status::Ok();
}

void Ignore::addPattern(const std::string& raw) {
  std::string line = trim(raw);
  if (line.empty() || line[0] == '#') return;
  Rule r{line, false};
  if (r.glob.back() == '/') {
    r.dirOnly = true;
    r.glob.pop_back();
  }
  while (!r.glob.empty() && r.glob[0] == '/') r.glob.erase(0, 1);
  if (!r.glob.empty()) rules_.push_back(r);
}

bool Ignore::matchOne(const std::string& rel, bool isDir) const {
  std::string base = fsu::baseName(rel);
  for (const Rule& r : rules_) {
    if (r.dirOnly && !isDir) continue;
    if (fnmatch(r.glob.c_str(), rel.c_str(), 0) == 0 || fnmatch(r.glob.c_str(), base.c_str(), 0) == 0)
      return true;
  }
  return false;
}

bool Ignore::ignored(const std::string& rel, bool isDir) const {
  if (rel == ".chronicle" || startsWith(rel, ".chronicle/")) return true;
  for (size_t pos = rel.find('/'); pos != std::string::npos; pos = rel.find('/', pos + 1))
    if (matchOne(rel.substr(0, pos), true)) return true;
  return matchOne(rel, isDir);
}

}
