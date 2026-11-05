#include <cstdio>

#include "cli/common.hpp"
#include "engine/worktree.hpp"
#include "store/lock.hpp"
#include "store/state.hpp"

namespace chr {
namespace cli {

int cmdRm(const std::vector<std::string>& a) {
  ArgParser ap(a);
  bool cached = ap.flag("--cached");
  CHR_CLI_TRY(ap.finish());
  std::vector<std::string> paths = ap.positional;
  paths.insert(paths.end(), ap.afterDashDash.begin(), ap.afterDashDash.end());
  if (paths.empty()) return report(UsageErr("usage: chronicle rm [--cached] <path>..."));
  Repo repo;
  CHR_CLI_TRY(openRepo(&repo));
  RepoLock lock;
  CHR_CLI_TRY(lock.acquire(repo.dir));
  Index index;
  CHR_CLI_TRY(index.load(repo.path("index")));
  MergeState ms;
  CHR_CLI_TRY(ms.load(repo.dir));
  std::vector<std::string> victims;
  for (const std::string& arg : paths) {
    std::string rel;
    CHR_CLI_TRY(toRepoRel(repo, arg, &rel));
    std::string pre = rel.empty() ? "" : rel + "/";
    size_t before = victims.size();
    if (index.entries.count(rel)) victims.push_back(rel);
    for (auto it = index.entries.lower_bound(pre); it != index.entries.end() && startsWith(it->first, pre); ++it)
      victims.push_back(it->first);
    if (victims.size() == before) return report(Err("pathspec '" + arg + "' did not match any tracked files"));
  }
  for (const std::string& v : victims) {
    index.entries.erase(v);
    ms.markResolved(v);
    if (!cached) CHR_CLI_TRY(removeWorkFile(repo, v));
    std::printf("rm '%s'\n", displayPath(v).c_str());
  }
  CHR_CLI_TRY(index.save(repo.path("index")));
  if (ms.active) CHR_CLI_TRY(ms.save(repo.dir));
  return 0;
}

}
}
