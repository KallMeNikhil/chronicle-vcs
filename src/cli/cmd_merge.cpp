#include <cstdio>

#include "cli/common.hpp"
#include "engine/commitops.hpp"
#include "engine/dag.hpp"
#include "engine/merge.hpp"
#include "engine/revparse.hpp"
#include "store/lock.hpp"
#include "store/state.hpp"

namespace chr {
namespace cli {

static Status requireClean(Repo& repo, const Index& index) {
  StatusReport st;
  CHR_TRY(computeStatus(repo, index, &st, false));
  if (st.clean()) return Status::Ok();
  return Err("cannot merge with uncommitted changes; commit them or run 'chronicle reset --hard'");
}

Status runThreeWay(Repo& repo, Index& index, const Id* base, const Id& oursCommit, const Id& theirsCommit,
                   const std::string& oursLabel, const std::string& theirsLabel, const std::string& kind,
                   const std::string& message, const std::vector<Id>& parents, Id* made) {
  Id oursTree, theirsTree;
  CHR_TRY(commitTreeId(repo.store, oursCommit, &oursTree));
  CHR_TRY(commitTreeId(repo.store, theirsCommit, &theirsTree));
  MergeOutcome m;
  CHR_TRY(mergeTrees(repo.store, base, &oursTree, &theirsTree, oursLabel, theirsLabel, &m));
  FlatTree ours;
  CHR_TRY(flattenTree(repo.store, oursTree, &ours));
  CHR_TRY(applyMergeOutcome(repo, index, ours, m));
  CHR_TRY(index.save(repo.path("index")));
  if (!m.clean()) {
    MergeState ms;
    ms.active = true;
    ms.kind = kind;
    ms.theirs = theirsCommit;
    ms.branch = theirsLabel;
    ms.message = message;
    for (const auto& c : m.conflicts) {
      std::printf("CONFLICT (%s): %s\n", c.first.c_str(), displayPath(c.second).c_str());
      ms.conflicts.push_back(c.second);
    }
    CHR_TRY(ms.save(repo.dir));
    return ConflictErr("automatic " + kind + " failed; fix conflicts, 'chronicle add' them, then 'chronicle commit'");
  }
  Id tree;
  CHR_TRY(index.buildTree(repo.store, &tree));
  CHR_TRY(createCommit(repo, tree, parents, message, made));
  return recordCommit(repo, *made);
}

int cmdMerge(const std::vector<std::string>& a) {
  ArgParser ap(a);
  bool noff = ap.flag("--no-ff"), abort = ap.flag("--abort"), unrelated = ap.flag("--allow-unrelated");
  CHR_CLI_TRY(ap.finish());
  Repo repo;
  CHR_CLI_TRY(openRepo(&repo));
  RepoLock lock;
  CHR_CLI_TRY(lock.acquire(repo.dir));
  MergeState ms;
  CHR_CLI_TRY(ms.load(repo.dir));
  if (abort) {
    if (!ap.positional.empty()) return report(UsageErr("usage: chronicle merge --abort"));
    if (!ms.active) return report(Err("no merge in progress"));
    Id head;
    CHR_CLI_TRY(resolveRevision(repo, "HEAD", &head));
    CHR_CLI_TRY(resetHardTo(repo, head));
    std::printf("Merge aborted; HEAD is at %s\n", head.shortHex().c_str());
    return 0;
  }
  if (ap.positional.size() != 1)
    return report(UsageErr("usage: chronicle merge <branch> [--no-ff] [--abort] [--allow-unrelated]"));
  if (ms.active) return report(Err("a merge is already in progress (commit it or 'chronicle merge --abort')"));
  const std::string name = ap.positional[0];
  HeadInfo head;
  CHR_CLI_TRY(repo.refs.readHead(&head));
  if (head.unborn) return report(Err("cannot merge into a branch with no commits"));
  Id theirs;
  CHR_CLI_TRY(resolveRevision(repo, name, &theirs));
  Index index;
  CHR_CLI_TRY(index.load(repo.path("index")));
  CHR_CLI_TRY(requireClean(repo, index));

  Dag dag;
  CHR_CLI_TRY(dag.load(repo.store, {head.id, theirs}));
  int o = dag.idxOf(head.id), t = dag.idxOf(theirs);
  if (dag.isAncestor(t, o)) {
    std::printf("Already up to date.\n");
    return 0;
  }
  if (dag.isAncestor(o, t) && !noff) {
    FlatTree from, to;
    CHR_CLI_TRY(flattenCommit(repo.store, head.id, &from));
    CHR_CLI_TRY(flattenCommit(repo.store, theirs, &to));
    CHR_CLI_TRY(switchTree(repo, index, from, to));
    CHR_CLI_TRY(index.save(repo.path("index")));
    CHR_CLI_TRY(repo.refs.advanceHead(theirs));
    std::printf("Fast-forward %s..%s\n", head.id.shortHex().c_str(), theirs.shortHex().c_str());
    return 0;
  }
  std::vector<int> bases = dag.mergeBases(o, t);
  Id baseTree;
  const Id* basePtr = nullptr;
  if (bases.empty()) {
    if (!unrelated) return report(Err("refusing to merge unrelated histories (use --allow-unrelated)"));
  } else {
    int b = dag.chooseBase(bases);
    if (bases.size() > 1)
      std::fprintf(stderr, "warning: %zu merge bases, using %s\n", bases.size(), dag.node(b).id.shortHex().c_str());
    CHR_CLI_TRY(commitTreeId(repo.store, dag.node(b).id, &baseTree));
    basePtr = &baseTree;
  }
  std::string oursLabel = head.detached ? "HEAD" : head.branch;
  Id made;
  CHR_CLI_TRY(runThreeWay(repo, index, basePtr, head.id, theirs, oursLabel, name, "merge",
                          "Merge branch '" + name + "'", {head.id, theirs}, &made));
  std::printf("Merge made by the three-way strategy: %s\n", made.shortHex().c_str());
  return 0;
}

}
}
