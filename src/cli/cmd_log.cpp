#include <climits>
#include <cstdio>
#include <unordered_set>

#include "cli/common.hpp"
#include "core/timeutil.hpp"
#include "engine/graphview.hpp"
#include "engine/logwalk.hpp"
#include "engine/history.hpp"
#include "engine/revparse.hpp"
#include "engine/timetravel.hpp"

namespace chr {
namespace cli {

Status printLog(Repo& repo, const Dag& dag, const std::vector<int>& order, const LogOptions& opt) {
  std::unordered_map<Id, std::string, IdHash> deco;
  CHR_TRY(decorations(repo, &deco));
  HexTrie trie;
  if (opt.oneline || opt.graph) CHR_TRY(buildIdTrie(repo.store, &trie));
  GraphLayout layout(dag);
  int64_t shown = 0;
  for (int x : order) {
    if (opt.limit >= 0 && shown >= opt.limit) break;
    ++shown;
    const Id& id = dag.node(x).id;
    Commit c;
    CHR_TRY(repo.store.readCommit(id, &c));
    GraphRow row;
    if (opt.graph) row = layout.next(x);
    for (const std::string& r : row.before) std::printf("%s\n", r.c_str());
    auto d = deco.find(id);
    std::string decoText = d == deco.end() ? "" : " (" + d->second + ")";
    if (opt.oneline || opt.graph) {
      std::printf("%s%s%s %s\n", row.prefix.c_str(), shortIdFor(trie, id, static_cast<size_t>(repo.abbrev())).c_str(),
                  decoText.c_str(), c.summary().c_str());
      if (!opt.oneline) {
        std::printf("%s  Author: %s\n", row.continuation.c_str(), c.author.c_str());
      }
    } else {
      std::printf("commit %s%s\n", id.hex().c_str(), decoText.c_str());
      std::vector<std::string> lines = split(c.message, '\n');
      if (!lines.empty() && lines.back().empty()) lines.pop_back();
      if (c.parents.size() > 1) {
        std::printf("Merge:");
        for (const Id& p : c.parents) std::printf(" %s", p.shortHex().c_str());
        std::printf("\n");
      }
      std::printf("Author: %s\nDate:   %s\n\n", c.author.c_str(), formatDateTime(c.time, c.tz).c_str());
      for (const std::string& l : lines) std::printf("%s%s\n", l.empty() ? "" : "    ", l.c_str());
      std::printf("\n");
    }
    for (const std::string& r : row.after) std::printf("%s\n", r.c_str());
  }
  return Status::Ok();
}

int cmdLog(const std::vector<std::string>& a) {
  ArgParser ap(a);
  LogOptions opt;
  opt.oneline = ap.flag("--oneline");
  opt.graph = ap.flag("--graph");
  std::string n, sinceArg, untilArg;
  if (ap.option("-n", &n) && (!parseInt64(n, &opt.limit) || opt.limit < 0))
    return report(UsageErr("-n needs a non-negative number"));
  int64_t since = INT64_MIN, until = INT64_MAX;
  bool haveSince = ap.option("--since", &sinceArg), haveUntil = ap.option("--until", &untilArg);
  if ((haveSince && !parseTimeSpec(sinceArg, &since)) || (haveUntil && !parseTimeSpec(untilArg, &until)))
    return report(UsageErr("bad time (use YYYY-MM-DD[THH:MM[:SS]] or Unix seconds)"));
  CHR_CLI_TRY(ap.finish());
  if (ap.positional.size() > 1) return report(UsageErr("usage: chronicle log [options] [<rev>] [-- <path>]"));
  Repo repo;
  CHR_CLI_TRY(openRepo(&repo));
  Id tip;
  CHR_CLI_TRY(resolveRevision(repo, ap.positional.empty() ? "HEAD" : ap.positional[0], &tip));
  Dag dag;
  CHR_CLI_TRY(dag.load(repo.store, {tip}));
  std::vector<int> order = dateOrderWalk(dag, {dag.idxOf(tip)});

  std::unordered_set<int> keep;
  bool filtered = false;
  if (haveSince || haveUntil) {
    BPlusTree<TimeKey, int> tree(64);
    buildTimeIndex(dag, &tree);
    for (const auto& kv : tree.rangeScan(TimeKey{since, INT32_MIN}, TimeKey{until, INT32_MAX})) keep.insert(kv.second);
    filtered = true;
  }
  if (ap.sawDashDash) {
    HistoryIndex hist;
    CHR_CLI_TRY(hist.load(repo));
    CHR_CLI_TRY(hist.ensureCovered(repo, dag));
    std::unordered_set<int> touching;
    for (const std::string& p : ap.afterDashDash) {
      std::string rel;
      CHR_CLI_TRY(toRepoRel(repo, p, &rel));
      for (int e : hist.forPrefix(rel)) {
        int d = dag.idxOf(hist.events()[static_cast<size_t>(e)].commit);
        if (d >= 0 && (!filtered || keep.count(d))) touching.insert(d);
      }
    }
    keep.swap(touching);
    filtered = true;
  }
  if (filtered) {
    std::vector<int> f;
    for (int x : order)
      if (keep.count(x)) f.push_back(x);
    order.swap(f);
  }
  CHR_CLI_TRY(printLog(repo, dag, order, opt));
  return 0;
}

}
}
