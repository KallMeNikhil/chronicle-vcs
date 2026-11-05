#include <cstdio>

#include "cli/common.hpp"
#include "engine/revparse.hpp"
#include "store/lock.hpp"

namespace chr {
namespace cli {

int cmdBranch(const std::vector<std::string>& a) {
  ArgParser ap(a);
  std::string del;
  bool doDelete = ap.option("-d", &del);
  bool list = ap.flag("--list");
  CHR_CLI_TRY(ap.finish());
  Repo repo;
  CHR_CLI_TRY(openRepo(&repo));
  HeadInfo head;
  CHR_CLI_TRY(repo.refs.readHead(&head));

  if (doDelete) {
    if (!ap.positional.empty()) return report(UsageErr("usage: chronicle branch -d <name>"));
    if (!head.detached && head.branch == del) return report(Err("cannot delete the current branch '" + del + "'"));
    RepoLock lock;
    CHR_CLI_TRY(lock.acquire(repo.dir));
    Id was;
    CHR_CLI_TRY(repo.refs.readBranch(del, &was));
    CHR_CLI_TRY(repo.refs.deleteBranch(del));
    std::printf("Deleted branch %s (was %s)\n", del.c_str(), was.shortHex().c_str());
    return 0;
  }
  if (list || ap.positional.empty()) {
    std::vector<std::pair<std::string, Id>> branches;
    CHR_CLI_TRY(repo.refs.listBranches(&branches));
    if (head.detached) std::printf("* (HEAD detached at %s)\n", head.id.shortHex().c_str());
    for (const auto& b : branches)
      std::printf("%s %s\n", (!head.detached && b.first == head.branch) ? "*" : " ", b.first.c_str());
    return 0;
  }
  if (ap.positional.size() > 2) return report(UsageErr("usage: chronicle branch <name> [<rev>]"));
  const std::string& name = ap.positional[0];
  if (!Refs::validBranchName(name)) return report(Err("invalid branch name '" + name + "'"));
  if (repo.refs.branchExists(name)) return report(Err("branch '" + name + "' already exists"));
  Id at;
  CHR_CLI_TRY(resolveRevision(repo, ap.positional.size() == 2 ? ap.positional[1] : "HEAD", &at));
  RepoLock lock;
  CHR_CLI_TRY(lock.acquire(repo.dir));
  CHR_CLI_TRY(repo.refs.writeBranch(name, at));
  return 0;
}

}
}
