#include <cstdio>

#include "cli/common.hpp"
#include "engine/revparse.hpp"
#include "engine/worktree.hpp"
#include "store/lock.hpp"
#include "store/state.hpp"

namespace chr {
namespace cli {

int cmdCheckout(const std::vector<std::string>& a) {
  ArgParser ap(a);
  std::string newBranch;
  bool create = ap.option("-b", &newBranch);
  CHR_CLI_TRY(ap.finish());
  if (ap.positional.size() > 1 || (!create && ap.positional.empty()))
    return report(UsageErr("usage: chronicle checkout <branch|rev> [-b <newbranch>]"));
  Repo repo;
  CHR_CLI_TRY(openRepo(&repo));
  RepoLock lock;
  CHR_CLI_TRY(lock.acquire(repo.dir));
  MergeState ms;
  CHR_CLI_TRY(ms.load(repo.dir));
  if (ms.active) return report(Err("a merge is in progress; commit it or run 'chronicle merge --abort'"));
  Index index;
  CHR_CLI_TRY(index.load(repo.path("index")));
  HeadInfo head;
  FlatTree from;
  CHR_CLI_TRY(headFlat(repo, &from, &head));

  if (create) {
    if (!Refs::validBranchName(newBranch)) return report(Err("invalid branch name '" + newBranch + "'"));
    if (repo.refs.branchExists(newBranch)) return report(Err("branch '" + newBranch + "' already exists"));
    if (head.unborn && ap.positional.empty()) {
      CHR_CLI_TRY(repo.refs.setHeadBranch(newBranch));
      std::printf("Switched to a new branch '%s'\n", newBranch.c_str());
      return 0;
    }
    Id start;
    CHR_CLI_TRY(resolveRevision(repo, ap.positional.empty() ? "HEAD" : ap.positional[0], &start));
    FlatTree to;
    CHR_CLI_TRY(flattenCommit(repo.store, start, &to));
    CHR_CLI_TRY(switchTree(repo, index, from, to));
    CHR_CLI_TRY(index.save(repo.path("index")));
    CHR_CLI_TRY(repo.refs.writeBranch(newBranch, start));
    CHR_CLI_TRY(repo.refs.setHeadBranch(newBranch));
    std::printf("Switched to a new branch '%s'\n", newBranch.c_str());
    return 0;
  }

  const std::string& target = ap.positional[0];
  bool isBranch = target.size() != 64 && repo.refs.branchExists(target);
  if (isBranch && !head.detached && head.branch == target) {
    std::printf("Already on '%s'\n", target.c_str());
    return 0;
  }
  Id id;
  CHR_CLI_TRY(isBranch ? repo.refs.readBranch(target, &id) : resolveRevision(repo, target, &id));
  FlatTree to;
  CHR_CLI_TRY(flattenCommit(repo.store, id, &to));
  CHR_CLI_TRY(switchTree(repo, index, from, to));
  CHR_CLI_TRY(index.save(repo.path("index")));
  if (isBranch) {
    CHR_CLI_TRY(repo.refs.setHeadBranch(target));
    std::printf("Switched to branch '%s'\n", target.c_str());
  } else {
    CHR_CLI_TRY(repo.refs.setHeadDetached(id));
    Commit c;
    CHR_CLI_TRY(repo.store.readCommit(id, &c));
    std::printf("HEAD is now at %s %s (detached)\n", id.shortHex().c_str(), c.summary().c_str());
  }
  return 0;
}

}
}
