#include <cstdio>

#include "cli/common.hpp"
#include "core/timeutil.hpp"
#include "engine/blame.hpp"
#include "engine/revparse.hpp"

namespace chr {
namespace cli {

int cmdBlame(const std::vector<std::string>& a) {
  ArgParser ap(a);
  CHR_CLI_TRY(ap.finish());
  if (ap.positional.empty() || ap.positional.size() > 2)
    return report(UsageErr("usage: chronicle blame <path> [<rev>]"));
  Repo repo;
  CHR_CLI_TRY(openRepo(&repo));
  std::string rel;
  CHR_CLI_TRY(toRepoRel(repo, ap.positional[0], &rel));
  Id rev;
  CHR_CLI_TRY(resolveRevision(repo, ap.positional.size() == 2 ? ap.positional[1] : "HEAD", &rev));
  Dag dag;
  CHR_CLI_TRY(dag.load(repo.store, {rev}));
  HistoryIndex hist;
  CHR_CLI_TRY(hist.load(repo));
  CHR_CLI_TRY(hist.ensureCovered(repo, dag));
  Blamer blamer(repo.store, dag, hist);
  std::vector<int> origins;
  std::vector<std::string> lines;
  CHR_CLI_TRY(blamer.blame(rev, rel, &origins, &lines));
  std::unordered_map<int, Commit> commits;
  int width = static_cast<int>(std::to_string(lines.size()).size());
  for (size_t i = 0; i < lines.size(); ++i) {
    int o = origins[i];
    if (!commits.count(o)) CHR_CLI_TRY(repo.store.readCommit(dag.node(o).id, &commits[o]));
    const Commit& c = commits[o];
    std::string text = lines[i];
    if (!text.empty() && text.back() == '\n') text.pop_back();
    std::printf("%s (%s %s) %*zu) %s\n", dag.node(o).id.shortHex().c_str(), c.authorName().c_str(),
                formatDate(c.time, c.tz).c_str(), width, i + 1, text.c_str());
  }
  return 0;
}

}
}
