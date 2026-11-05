#include <cstdio>

#include "cli/common.hpp"
#include "engine/timetravel.hpp"
#include "store/pack.hpp"
#include "util/fs.hpp"

namespace chr {
namespace cli {

int cmdStats(const std::vector<std::string>& a) {
  ArgParser ap(a);
  bool sharing = ap.flag("--sharing");
  std::string lastArg;
  int64_t last = 100;
  if (ap.option("--last", &lastArg) && (!parseInt64(lastArg, &last) || last <= 0))
    return report(UsageErr("--last needs a positive number"));
  CHR_CLI_TRY(ap.finish());
  if (!ap.positional.empty()) return report(UsageErr("usage: chronicle stats [--sharing] [--last N]"));
  Repo repo;
  CHR_CLI_TRY(openRepo(&repo));

  std::vector<Id> loose;
  CHR_CLI_TRY(repo.store.loose().list(&loose));
  uint64_t looseBytes = 0;
  for (const Id& id : loose) looseBytes += fsu::lstatPath(repo.store.loose().pathFor(id)).size;
  std::printf("loose objects:  %zu (%llu bytes)\n", loose.size(), static_cast<unsigned long long>(looseBytes));
  std::vector<Pack*> packs;
  CHR_CLI_TRY(repo.store.packs(&packs));
  size_t packed = 0, deltas = 0;
  uint64_t packBytes = 0, fullBytes = 0;
  for (Pack* p : packs) {
    packBytes += p->fileSize();
    for (const IdEntry& e : p->index()) {
      uint8_t kind = 0;
      uint64_t size = 0;
      CHR_CLI_TRY(p->entryInfo(e.offset, &kind, &size));
      ++packed;
      if (kind == kPackDelta) ++deltas;
      fullBytes += size + 16;
    }
  }
  std::printf("packs:          %zu\n", packs.size());
  std::printf("packed objects: %zu (%zu stored as deltas)\n", packed, deltas);
  if (packed)
    std::printf("pack bytes:     %llu for ~%llu bytes of objects (%.1f%%)\n",
                static_cast<unsigned long long>(packBytes), static_cast<unsigned long long>(fullBytes),
                100.0 * static_cast<double>(packBytes) / static_cast<double>(fullBytes));

  if (sharing) {
    SharingReport r;
    CHR_CLI_TRY(sharingReport(repo, static_cast<size_t>(last), &r));
    double ratio = r.uniqueObjects ? static_cast<double>(r.snapshotEntries) / static_cast<double>(r.uniqueObjects) : 0;
    std::printf("structural sharing over the last %zu commits:\n", r.commits);
    std::printf("  entries if every commit were a full snapshot: %llu\n",
                static_cast<unsigned long long>(r.snapshotEntries));
    std::printf("  unique tree + blob objects actually stored:   %llu\n",
                static_cast<unsigned long long>(r.uniqueObjects));
    std::printf("  sharing ratio: %.2fx\n", ratio);
  }
  return 0;
}

}
}
