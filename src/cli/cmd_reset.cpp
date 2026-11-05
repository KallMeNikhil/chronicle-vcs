#include <cstdio>

#include "cli/common.hpp"
#include "engine/revparse.hpp"
#include "engine/worktree.hpp"
#include "store/lock.hpp"
#include "store/state.hpp"

namespace chr {
namespace cli {

Status resetHardTo(Repo& repo, const Id& target) {
  Index index;
  if (!index.load(repo.path("index")).ok()) index = Index();
  FlatTree tracked;
  CHR_TRY(headFlat(repo, &tracked));
  for (const auto& kv : index.entries) tracked[kv.first] = FlatEntry{kv.second.mode, kv.second.id};
  FlatTree to;
  CHR_TRY(flattenCommit(repo.store, target, &to));
  CHR_TRY(hardResetTree(repo, index, tracked, to));
  CHR_TRY(index.save(repo.path("index")));
  CHR_TRY(repo.refs.advanceHead(target));
  return MergeState::remove(repo.dir);
}

int cmdReset(const std::vector<std::string>& a) {
  ArgParser ap(a);
  bool hard = ap.flag("--hard");
  CHR_CLI_TRY(ap.finish());
  if (!hard || ap.positional.size() > 1) return report(UsageErr("usage: chronicle reset --hard [<rev>]"));
  Repo repo;
  CHR_CLI_TRY(openRepo(&repo));
  RepoLock lock;
  CHR_CLI_TRY(lock.acquire(repo.dir));
  Id target;
  CHR_CLI_TRY(resolveRevision(repo, ap.positional.empty() ? "HEAD" : ap.positional[0], &target));
  CHR_CLI_TRY(resetHardTo(repo, target));
  Commit c;
  CHR_CLI_TRY(repo.store.readCommit(target, &c));
  std::printf("HEAD is now at %s %s\n", target.shortHex().c_str(), c.summary().c_str());
  return 0;
}

}
}
