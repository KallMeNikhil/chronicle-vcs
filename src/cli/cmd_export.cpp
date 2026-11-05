#include <cstdio>

#include "cli/common.hpp"
#include "engine/timetravel.hpp"
#include "util/fs.hpp"

namespace chr {
namespace cli {

int cmdExport(const std::vector<std::string>& a) {
  ArgParser ap(a);
  CHR_CLI_TRY(ap.finish());
  if (ap.positional.size() != 2) return report(UsageErr("usage: chronicle export <rev-or-time> <dir>"));
  Repo repo;
  CHR_CLI_TRY(openRepo(&repo));
  Id commit;
  CHR_CLI_TRY(resolveRevOrTime(repo, ap.positional[0], &commit));
  std::string dir = fsu::absPath(ap.positional[1]);
  if (dir == repo.work || startsWith(repo.dir + "/", dir + "/") || startsWith(dir, repo.dir + "/"))
    return report(Err("refusing to export into the repository root or .chronicle"));
  size_t files = 0;
  CHR_CLI_TRY(exportTree(repo, commit, dir, &files));
  std::printf("exported %s (%zu files) to %s\n", commit.shortHex().c_str(), files, dir.c_str());
  return 0;
}

}
}
