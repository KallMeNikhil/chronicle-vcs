#include <cstdio>

#include "cli/common.hpp"
#include "exp/reconcile.hpp"
#include "store/lock.hpp"

namespace chr {
namespace cli {

int cmdReconcile(const std::vector<std::string>& a) {
  ArgParser ap(a);
  bool fetch = ap.flag("--fetch");
  CHR_CLI_TRY(ap.finish());
  if (ap.positional.size() != 1) return report(UsageErr("usage: chronicle reconcile <other-repo-dir> [--fetch]"));
  Repo repo, other;
  CHR_CLI_TRY(openRepo(&repo));
  CHR_CLI_TRY(other.open(ap.positional[0]));
  if (other.dir == repo.dir) return report(Err("reconcile: both paths are the same repository"));
  std::vector<BranchReport> reports;
  CHR_CLI_TRY(compareRepos(repo, other, &reports));
  for (const BranchReport& r : reports) {
    std::printf("%s: ", r.name.c_str());
    if (!r.there) {
      std::printf("only here\n");
    } else if (!r.here) {
      std::printf("only in other (%zu commits)\n", r.behind);
    } else if (r.local == r.remote) {
      std::printf("identical\n");
    } else if (r.behind == 0) {
      std::printf("ahead %zu\n", r.ahead);
    } else if (r.ahead == 0) {
      std::printf("behind %zu\n", r.behind);
    } else {
      std::printf("diverged (ahead %zu, behind %zu)", r.ahead, r.behind);
      if (r.haveBase) std::printf(", merge base %s", r.base.shortHex().c_str());
      std::printf("\n");
    }
  }
  if (!fetch) return 0;
  RepoLock lock;
  CHR_CLI_TRY(lock.acquire(repo.dir));
  for (const BranchReport& r : reports) {
    if (!r.there) continue;
    size_t copied = 0;
    CHR_CLI_TRY(fetchObjects(repo, other, r.remote, &copied));
    std::string ref = r.name + "-remote";
    CHR_CLI_TRY(repo.refs.writeBranch(ref, r.remote));
    std::printf("fetched %s -> %s (%zu objects copied)\n", r.name.c_str(), ref.c_str(), copied);
  }
  return 0;
}

}
}
