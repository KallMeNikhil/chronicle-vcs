#include <cstdio>

#include "cli/common.hpp"
#include "engine/history.hpp"
#include "store/lock.hpp"

namespace chr {
namespace cli {

int cmdReindex(const std::vector<std::string>& a) {
  ArgParser ap(a);
  CHR_CLI_TRY(ap.finish());
  if (!ap.positional.empty()) return report(UsageErr("usage: chronicle reindex"));
  Repo repo;
  CHR_CLI_TRY(openRepo(&repo));
  RepoLock lock;
  CHR_CLI_TRY(lock.acquire(repo.dir));
  size_t n = 0;
  CHR_CLI_TRY(reindexHistory(repo, &n));
  std::printf("history.idx rebuilt: %zu events\n", n);
  return 0;
}

}
}
