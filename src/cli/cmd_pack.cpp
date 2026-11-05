#include <cstdio>

#include "cli/common.hpp"
#include "engine/gc.hpp"
#include "store/lock.hpp"

namespace chr {
namespace cli {

void printPackWrite(const PackWriteStats& s) {
  if (s.objects == 0) {
    std::printf("nothing to pack\n");
    return;
  }
  double pct = s.fullBytes ? 100.0 * static_cast<double>(s.packBytes) / static_cast<double>(s.fullBytes) : 0;
  std::printf("packed %zu objects (%zu as deltas) into %s\n", s.objects, s.deltas, s.name.c_str());
  std::printf("  %llu bytes as loose objects -> %llu bytes packed (%.1f%%)\n",
              static_cast<unsigned long long>(s.fullBytes), static_cast<unsigned long long>(s.packBytes), pct);
}

int cmdPack(const std::vector<std::string>& a) {
  ArgParser ap(a);
  bool keep = ap.flag("--keep-loose");
  CHR_CLI_TRY(ap.finish());
  if (!ap.positional.empty()) return report(UsageErr("usage: chronicle pack [--keep-loose]"));
  Repo repo;
  CHR_CLI_TRY(openRepo(&repo));
  RepoLock lock;
  CHR_CLI_TRY(lock.acquire(repo.dir));
  PackWriteStats s;
  CHR_CLI_TRY(packLoose(repo, keep, &s));
  printPackWrite(s);
  return 0;
}

}
}
