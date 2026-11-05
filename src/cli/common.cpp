#include "cli/common.hpp"

#include <cstdio>

#include "core/strutil.hpp"
#include "engine/revparse.hpp"
#include "util/fs.hpp"

namespace chr {
namespace cli {

Status resolveObject(Repo& repo, const std::string& name, Id* out) { return resolveAnyObject(repo, name, out); }

void printTreeEntries(const std::vector<TreeEntry>& entries, const std::string& prefix) {
  for (const TreeEntry& e : entries) {
    const char* type = e.isDir() ? "tree" : "blob";
    std::printf("%s %s %s\t%s\n", modeString6(e.mode).c_str(), type, e.id.hex().c_str(),
                displayPath(prefix + e.name).c_str());
  }
}

Status toRepoRel(const Repo& repo, const std::string& arg, std::string* rel) {
  std::string abs = fsu::absPath(arg);
  if (abs == repo.work) {
    rel->clear();
  } else if (startsWith(abs, repo.work + "/") || repo.work == "/") {
    *rel = abs.substr(repo.work == "/" ? 1 : repo.work.size() + 1);
  } else {
    return Err("path '" + arg + "' is outside the repository");
  }
  if (*rel == ".chronicle" || startsWith(*rel, ".chronicle/"))
    return Err("path '" + arg + "' is inside the .chronicle directory");
  return Status::Ok();
}

Status openRepo(Repo* repo) { return repo->open("."); }

Status decorations(Repo& repo, std::unordered_map<Id, std::string, IdHash>* out) {
  out->clear();
  HeadInfo h;
  CHR_TRY(repo.refs.readHead(&h));
  std::vector<std::pair<std::string, Id>> branches;
  CHR_TRY(repo.refs.listBranches(&branches));
  if (h.detached) (*out)[h.id] = "HEAD";
  for (const auto& b : branches) {
    std::string label = (!h.detached && b.first == h.branch) ? "HEAD -> " + b.first : b.first;
    std::string& cur = (*out)[b.second];
    if (startsWith(label, "HEAD")) cur = cur.empty() ? label : label + ", " + cur;
    else cur = cur.empty() ? label : cur + ", " + label;
  }
  return Status::Ok();
}

}
}
