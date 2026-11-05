#include <cstdio>

#include "cli/common.hpp"
#include "engine/timetravel.hpp"
#include "engine/treediff.hpp"

namespace chr {
namespace cli {

int cmdAt(const std::vector<std::string>& a) {
  ArgParser ap(a);
  CHR_CLI_TRY(ap.finish());
  if (ap.positional.size() != 2) return report(UsageErr("usage: chronicle at <rev-or-time> <path>"));
  Repo repo;
  CHR_CLI_TRY(openRepo(&repo));
  Id commit;
  CHR_CLI_TRY(resolveRevOrTime(repo, ap.positional[0], &commit));
  std::string rel;
  CHR_CLI_TRY(toRepoRel(repo, ap.positional[1], &rel));
  Commit c;
  CHR_CLI_TRY(repo.store.readCommit(commit, &c));
  TreeEntry e;
  bool found = false;
  CHR_CLI_TRY(lookupPath(repo.store, c.tree, rel, &e, &found));
  if (!found) return report(Err("'" + rel + "' does not exist at " + commit.shortHex()));
  if (e.isDir()) {
    std::vector<TreeEntry> entries;
    CHR_CLI_TRY(repo.store.readTree(e.id, &entries));
    printTreeEntries(entries, rel.empty() ? "" : rel + "/");
    return 0;
  }
  std::string content;
  CHR_CLI_TRY(repo.store.readBlob(e.id, &content));
  std::fwrite(content.data(), 1, content.size(), stdout);
  return 0;
}

}
}
