#include "cli/common.hpp"
#include "engine/revparse.hpp"
#include "engine/treediff.hpp"

namespace chr {
namespace cli {

static Status listRecursive(Repo& repo, const Id& tree, const std::string& prefix) {
  std::vector<TreeEntry> entries;
  CHR_TRY(repo.store.readTree(tree, &entries));
  for (const TreeEntry& e : entries) {
    if (e.isDir())
      CHR_TRY(listRecursive(repo, e.id, prefix + e.name + "/"));
    else
      printTreeEntries({e}, prefix);
  }
  return Status::Ok();
}

int cmdLsTree(const std::vector<std::string>& a) {
  ArgParser ap(a);
  bool recursive = ap.flag("-r");
  CHR_CLI_TRY(ap.finish());
  if (ap.positional.empty() || ap.positional.size() > 2)
    return report(UsageErr("usage: chronicle ls-tree [-r] <rev> [<path>]"));
  Repo repo;
  CHR_CLI_TRY(openRepo(&repo));
  Id id;
  CHR_CLI_TRY(resolveObject(repo, ap.positional[0], &id));
  ObjType t;
  std::string payload;
  CHR_CLI_TRY(repo.store.get(id, &t, &payload));
  Id tree = id;
  if (t == ObjType::Commit) CHR_CLI_TRY(commitTreeId(repo.store, id, &tree));
  else if (t != ObjType::Tree) return report(Err("'" + ap.positional[0] + "' is not a tree or commit"));
  std::string prefix;
  if (ap.positional.size() == 2) {
    std::string path = ap.positional[1];
    while (!path.empty() && path.back() == '/') path.pop_back();
    TreeEntry e;
    bool found = false;
    CHR_CLI_TRY(lookupPath(repo.store, tree, path, &e, &found));
    if (!found) return report(Err("path '" + path + "' does not exist in '" + ap.positional[0] + "'"));
    if (!e.isDir()) {
      e.name = path;
      printTreeEntries({e}, "");
      return 0;
    }
    tree = e.id;
    prefix = path.empty() ? "" : path + "/";
  }
  if (recursive) {
    CHR_CLI_TRY(listRecursive(repo, tree, prefix));
  } else {
    std::vector<TreeEntry> entries;
    CHR_CLI_TRY(repo.store.readTree(tree, &entries));
    printTreeEntries(entries, prefix);
  }
  return 0;
}

}
}
