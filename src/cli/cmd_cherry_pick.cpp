#include <cstdio>

#include "cli/common.hpp"
#include "engine/revparse.hpp"
#include "engine/worktree.hpp"
#include "exp/cherrypick.hpp"
#include "store/lock.hpp"
#include "store/state.hpp"

namespace chr {
namespace cli {

int cmdCherryPick(const std::vector<std::string>& a) {
  ArgParser ap(a);
  CHR_CLI_TRY(ap.finish());
  if (ap.positional.size() != 1) return report(UsageErr("usage: chronicle cherry-pick <rev>"));
  Repo repo;
  CHR_CLI_TRY(openRepo(&repo));
  RepoLock lock;
  CHR_CLI_TRY(lock.acquire(repo.dir));
  MergeState ms;
  CHR_CLI_TRY(ms.load(repo.dir));
  if (ms.active) return report(Err("a merge or cherry-pick is in progress"));
  HeadInfo head;
  CHR_CLI_TRY(repo.refs.readHead(&head));
  if (head.unborn) return report(Err("cannot cherry-pick onto a branch with no commits"));
  Id rev;
  CHR_CLI_TRY(resolveRevision(repo, ap.positional[0], &rev));
  Commit c;
  CHR_CLI_TRY(repo.store.readCommit(rev, &c));
  if (c.parents.size() > 1) return report(Err("cannot cherry-pick a merge commit"));

  bool found = false;
  Id as;
  CHR_CLI_TRY(findAppliedEquivalent(repo, rev, head.id, &found, &as));
  if (found) {
    std::printf("already applied as %s\n", as.shortHex().c_str());
    return 0;
  }
  Index index;
  CHR_CLI_TRY(index.load(repo.path("index")));
  StatusReport st;
  CHR_CLI_TRY(computeStatus(repo, index, &st, false));
  if (!st.clean()) return report(Err("cannot cherry-pick with uncommitted changes"));
  Id baseTree;
  if (!c.parents.empty()) CHR_CLI_TRY(commitTreeId(repo.store, c.parents[0], &baseTree));
  Id made;
  CHR_CLI_TRY(runThreeWay(repo, index, c.parents.empty() ? nullptr : &baseTree, head.id, rev,
                          head.detached ? "HEAD" : head.branch, rev.shortHex(), "cherry-pick", c.message,
                          {head.id}, &made));
  std::printf("[%s %s] %s\n", head.detached ? "detached HEAD" : head.branch.c_str(), made.shortHex().c_str(),
              c.summary().c_str());
  return 0;
}

}
}
