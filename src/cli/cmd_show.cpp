#include <cstdio>

#include "cli/common.hpp"
#include "cli/diffprint.hpp"
#include "core/timeutil.hpp"
#include "engine/revparse.hpp"

namespace chr {
namespace cli {

Status printCommitHeader(const Id& id, const Commit& c) {
  std::printf("commit %s\n", id.hex().c_str());
  if (c.parents.size() > 1) {
    std::printf("Merge:");
    for (const Id& p : c.parents) std::printf(" %s", p.shortHex().c_str());
    std::printf("\n");
  }
  std::printf("Author: %s\nDate:   %s\n\n", c.author.c_str(), formatDateTime(c.time, c.tz).c_str());
  std::vector<std::string> lines = split(c.message, '\n');
  if (!lines.empty() && lines.back().empty()) lines.pop_back();
  for (const std::string& line : lines) std::printf("%s%s\n", line.empty() ? "" : "    ", line.c_str());
  return Status::Ok();
}

int cmdShow(const std::vector<std::string>& a) {
  ArgParser ap(a);
  CHR_CLI_TRY(ap.finish());
  if (ap.positional.size() > 1) return report(UsageErr("usage: chronicle show <rev-or-id>"));
  Repo repo;
  CHR_CLI_TRY(openRepo(&repo));
  Id id;
  CHR_CLI_TRY(resolveObject(repo, ap.positional.empty() ? "HEAD" : ap.positional[0], &id));
  ObjType t;
  std::string payload;
  CHR_CLI_TRY(repo.store.get(id, &t, &payload));
  if (t == ObjType::Blob) {
    std::fwrite(payload.data(), 1, payload.size(), stdout);
    return 0;
  }
  if (t == ObjType::Tree) {
    std::vector<TreeEntry> entries;
    CHR_CLI_TRY(decodeTree(payload, &entries));
    printTreeEntries(entries, "");
    return 0;
  }
  Commit c;
  CHR_CLI_TRY(decodeCommit(payload, &c));
  CHR_CLI_TRY(printCommitHeader(id, c));
  std::vector<TreeChange> changes;
  Id parentTree;
  if (!c.parents.empty()) CHR_CLI_TRY(commitTreeId(repo.store, c.parents[0], &parentTree));
  CHR_CLI_TRY(diffTrees(repo.store, c.parents.empty() ? nullptr : &parentTree, &c.tree, &changes));
  if (!changes.empty()) std::printf("\n");
  CHR_CLI_TRY(printChanges(repo, changes, false, {}));
  return 0;
}

}
}
