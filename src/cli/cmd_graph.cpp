#include <cstdio>

#include "cli/common.hpp"
#include "engine/graphview.hpp"
#include "engine/logwalk.hpp"
#include "engine/revparse.hpp"

namespace chr {
namespace cli {

int cmdGraph(const std::vector<std::string>& a) {
  ArgParser ap(a);
  bool dot = ap.flag("--dot");
  CHR_CLI_TRY(ap.finish());
  Repo repo;
  CHR_CLI_TRY(openRepo(&repo));
  std::vector<Id> tips;
  if (ap.positional.empty()) {
    CHR_CLI_TRY(refTips(repo, &tips));
  } else {
    for (const std::string& r : ap.positional) {
      Id id;
      CHR_CLI_TRY(resolveRevision(repo, r, &id));
      tips.push_back(id);
    }
  }
  if (tips.empty()) return report(Err("no commits yet"));
  Dag dag;
  CHR_CLI_TRY(dag.load(repo.store, tips));
  std::vector<int> tipIdx;
  for (const Id& t : tips) tipIdx.push_back(dag.idxOf(t));
  std::vector<int> order = dateOrderWalk(dag, tipIdx);
  if (!dot) {
    LogOptions opt;
    opt.oneline = true;
    opt.graph = true;
    CHR_CLI_TRY(printLog(repo, dag, order, opt));
    return 0;
  }
  std::vector<std::string> summaries;
  for (int x : order) {
    Commit c;
    CHR_CLI_TRY(repo.store.readCommit(dag.node(x).id, &c));
    summaries.push_back(c.summary());
  }
  std::vector<std::pair<std::string, Id>> branches;
  CHR_CLI_TRY(repo.refs.listBranches(&branches));
  std::vector<std::pair<std::string, int>> branchTips;
  for (const auto& b : branches)
    if (dag.idxOf(b.second) >= 0) branchTips.push_back({b.first, dag.idxOf(b.second)});
  std::string text = dotGraph(dag, order, branchTips, summaries);
  std::fwrite(text.data(), 1, text.size(), stdout);
  return 0;
}

}
}
