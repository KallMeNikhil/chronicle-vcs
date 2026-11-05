#include <cstdio>

#include "cli/common.hpp"
#include "engine/gc.hpp"
#include "store/lock.hpp"

namespace chr {
namespace cli {

void printPackWrite(const PackWriteStats& s);

int cmdGc(const std::vector<std::string>& a) {
  ArgParser ap(a);
  bool dry = ap.flag("--dry-run");
  std::string graceArg;
  bool haveGrace = ap.option("--grace-seconds", &graceArg);
  CHR_CLI_TRY(ap.finish());
  if (!ap.positional.empty()) return report(UsageErr("usage: chronicle gc [--dry-run] [--grace-seconds N]"));
  Repo repo;
  CHR_CLI_TRY(openRepo(&repo));
  int64_t grace = repo.config.getInt("gc.graceseconds", 0);
  if (haveGrace && (!parseInt64(graceArg, &grace) || grace < 0))
    return report(UsageErr("--grace-seconds needs a non-negative number"));
  RepoLock lock;
  CHR_CLI_TRY(lock.acquire(repo.dir));
  GcReport r;
  CHR_CLI_TRY(runGc(repo, dry, grace, &r));
  std::printf("%sreachable objects: %zu\n", dry ? "[dry run] " : "", r.reachable);
  std::printf("%sunreachable loose objects %s: %zu (kept by grace period: %zu)\n", dry ? "[dry run] " : "",
              dry ? "to delete" : "deleted", r.looseDeleted, r.looseKeptGrace);
  std::printf("%sunreachable packed objects %s: %zu\n", dry ? "[dry run] " : "", dry ? "to drop" : "dropped",
              r.packedDropped);
  std::printf("%sloose objects %s: %zu; old packs %s: %zu\n", dry ? "[dry run] " : "",
              dry ? "to repack" : "repacked", r.looseRepacked, dry ? "to replace" : "replaced", r.oldPacks);
  if (!dry) printPackWrite(r.pack);
  return 0;
}

}
}
