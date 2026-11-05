#include "cli/common.hpp"
#include "cli/diffprint.hpp"
#include "engine/revparse.hpp"

namespace chr {
namespace cli {

static Status worktreeSide(Repo& repo, const Index& index, FlatTree* out) {
  for (const auto& kv : index.entries) {
    fsu::FileStat st = fsu::lstatPath(repo.work + "/" + kv.first);
    if (!st.exists || st.isDir) continue;
    if (statClean(kv.second, st, index.writeTimeNs)) {
      (*out)[kv.first] = FlatEntry{kv.second.mode, kv.second.id};
      continue;
    }
    IndexEntry e;
    CHR_TRY(hashWorkFile(repo, kv.first, st, false, &e));
    (*out)[kv.first] = FlatEntry{e.mode, e.id};
  }
  return Status::Ok();
}

int cmdDiff(const std::vector<std::string>& a) {
  ArgParser ap(a);
  bool cached = ap.flag("--cached");
  CHR_CLI_TRY(ap.finish());
  if (ap.positional.size() > 2 || (cached && !ap.positional.empty()))
    return report(UsageErr("usage: chronicle diff [--cached] [<rev> [<rev>]] [-- <path>]"));
  Repo repo;
  CHR_CLI_TRY(openRepo(&repo));
  std::vector<std::string> filters;
  for (const std::string& f : ap.afterDashDash) {
    std::string rel;
    CHR_CLI_TRY(toRepoRel(repo, f, &rel));
    filters.push_back(rel);
  }
  std::vector<TreeChange> changes;
  bool fromWork = false;
  if (ap.positional.size() == 2) {
    Id c1, c2, t1, t2;
    CHR_CLI_TRY(resolveRevision(repo, ap.positional[0], &c1));
    CHR_CLI_TRY(resolveRevision(repo, ap.positional[1], &c2));
    CHR_CLI_TRY(commitTreeId(repo.store, c1, &t1));
    CHR_CLI_TRY(commitTreeId(repo.store, c2, &t2));
    CHR_CLI_TRY(diffTrees(repo.store, &t1, &t2, &changes));
  } else {
    Index index;
    CHR_CLI_TRY(index.load(repo.path("index")));
    FlatTree oldSide, newSide;
    if (cached) {
      CHR_CLI_TRY(headFlat(repo, &oldSide));
      newSide = indexFlat(index);
    } else {
      if (ap.positional.size() == 1) {
        Id c;
        CHR_CLI_TRY(resolveRevision(repo, ap.positional[0], &c));
        CHR_CLI_TRY(flattenCommit(repo.store, c, &oldSide));
      } else {
        oldSide = indexFlat(index);
      }
      CHR_CLI_TRY(worktreeSide(repo, index, &newSide));
      fromWork = true;
    }
    changes = diffFlat(oldSide, newSide);
  }
  CHR_CLI_TRY(printChanges(repo, changes, fromWork, filters));
  return 0;
}

}
}
