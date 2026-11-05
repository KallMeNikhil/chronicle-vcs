#include <cstdio>

#include "cli/common.hpp"
#include "engine/commitops.hpp"
#include "engine/worktree.hpp"
#include "store/lock.hpp"
#include "store/state.hpp"

namespace chr {
namespace cli {

int cmdCommit(const std::vector<std::string>& a) {
  ArgParser ap(a);
  std::string message;
  bool haveMsg = ap.option("-m", &message);
  CHR_CLI_TRY(ap.finish());
  if (!ap.positional.empty()) return report(UsageErr("usage: chronicle commit -m <message>"));
  Repo repo;
  CHR_CLI_TRY(openRepo(&repo));
  RepoLock lock;
  CHR_CLI_TRY(lock.acquire(repo.dir));
  Index index;
  CHR_CLI_TRY(index.load(repo.path("index")));
  HeadInfo head;
  CHR_CLI_TRY(repo.refs.readHead(&head));
  MergeState ms;
  CHR_CLI_TRY(ms.load(repo.dir));

  std::vector<Id> parents;
  if (!head.unborn) parents.push_back(head.id);
  if (ms.active) {
    if (!ms.conflicts.empty()) {
      std::string msg = "cannot commit: unresolved conflicts (edit, then 'chronicle add'):";
      for (const std::string& c : ms.conflicts) msg += "\n  " + displayPath(c);
      return report(Err(msg));
    }
    if (ms.kind == "merge") parents.push_back(ms.theirs);
    if (!haveMsg) message = ms.message;
  } else if (!haveMsg) {
    return report(UsageErr("commit needs a message: chronicle commit -m <message>"));
  }
  if (message.empty()) return report(UsageErr("empty commit message"));

  Id tree;
  CHR_CLI_TRY(index.buildTree(repo.store, &tree));
  if (!ms.active) {
    if (head.unborn && index.entries.empty())
      return report(Err("nothing to commit (create files and use 'chronicle add')"));
    if (!head.unborn) {
      Id headTree;
      CHR_CLI_TRY(commitTreeId(repo.store, head.id, &headTree));
      if (headTree == tree) return report(Err("nothing to commit (use 'chronicle add' to stage changes)"));
    }
  }
  Id id;
  CHR_CLI_TRY(createCommit(repo, tree, parents, message, &id));
  CHR_CLI_TRY(recordCommit(repo, id));
  if (ms.active) CHR_CLI_TRY(MergeState::remove(repo.dir));
  std::string summary = message.substr(0, message.find('\n'));
  std::printf("[%s %s] %s\n", head.detached ? "detached HEAD" : head.branch.c_str(), id.shortHex().c_str(),
              summary.c_str());
  if (head.detached)
    std::fprintf(stderr, "warning: committed on a detached HEAD; the commit is not on any branch\n");
  return 0;
}

}
}
