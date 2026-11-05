#include <algorithm>
#include <cstdio>
#include <unordered_map>

#include "cli/common.hpp"
#include "core/timeutil.hpp"
#include "engine/history.hpp"
#include "engine/logwalk.hpp"
#include "engine/revparse.hpp"

namespace chr {
namespace cli {

namespace {
struct Ctx {
  Repo repo;
  Dag dag;
  HistoryIndex hist;
  std::unordered_map<int, int> rank;

  Status load() {
    CHR_TRY(openRepo(&repo));
    std::vector<Id> tips;
    CHR_TRY(refTips(repo, &tips));
    if (tips.empty()) return Err("no commits yet");
    CHR_TRY(dag.load(repo.store, tips));
    CHR_TRY(hist.load(repo));
    CHR_TRY(hist.ensureCovered(repo, dag));
    std::vector<int> tipIdx;
    for (const Id& t : tips) tipIdx.push_back(dag.idxOf(t));
    std::vector<int> order = dateOrderWalk(dag, tipIdx);
    for (size_t i = 0; i < order.size(); ++i) rank[order[i]] = static_cast<int>(i);
    return Status::Ok();
  }
  std::vector<const HistEvent*> sorted(const std::vector<int>& ids) {
    std::vector<std::pair<int, const HistEvent*>> v;
    for (int e : ids) {
      const HistEvent& ev = hist.events()[static_cast<size_t>(e)];
      int d = dag.idxOf(ev.commit);
      if (d >= 0) v.push_back({rank[d], &ev});
    }
    std::stable_sort(v.begin(), v.end(),
                     [](const std::pair<int, const HistEvent*>& a, const std::pair<int, const HistEvent*>& b) {
                       return a.first < b.first;
                     });
    std::vector<const HistEvent*> out;
    for (auto& p : v) out.push_back(p.second);
    return out;
  }
  Status line(const HistEvent& e, const std::string& extra) {
    Commit c;
    CHR_TRY(repo.store.readCommit(e.commit, &c));
    std::printf("%c %s %s %s%s\n", e.kind, e.commit.shortHex().c_str(), formatDate(c.time, c.tz).c_str(),
                c.summary().c_str(), extra.c_str());
    return Status::Ok();
  }
};
}

int cmdHistory(const std::vector<std::string>& a) {
  ArgParser ap(a);
  bool first = ap.flag("--first");
  std::string blobArg;
  bool byBlob = ap.option("--blob", &blobArg);
  CHR_CLI_TRY(ap.finish());
  if ((byBlob && !ap.positional.empty()) || (!byBlob && ap.positional.size() != 1) || (first && byBlob))
    return report(UsageErr("usage: chronicle history <path> | --first <path> | --blob <id>"));
  Ctx ctx;
  CHR_CLI_TRY(ctx.load());

  if (byBlob) {
    Id blob;
    CHR_CLI_TRY(resolveAnyObject(ctx.repo, blobArg, &blob));
    const std::vector<int>* evs = ctx.hist.forBlob(blob);
    if (!evs) return report(Err("content " + blob.shortHex() + " does not appear in any commit"));
    std::vector<const HistEvent*> intro = ctx.sorted(*evs);
    for (auto it = intro.rbegin(); it != intro.rend(); ++it) {
      const HistEvent& e = **it;
      std::string gone = " -- still present";
      std::vector<const HistEvent*> onPath = ctx.sorted(*ctx.hist.forPath(e.path));
      for (size_t i = 0; i < onPath.size(); ++i) {
        if (onPath[i] != &e) continue;
        if (i > 0) gone = std::string(" -- ") + (onPath[i - 1]->kind == 'D' ? "deleted" : "replaced") + " in " +
                          onPath[i - 1]->commit.shortHex();
        break;
      }
      CHR_CLI_TRY(ctx.line(e, "  [" + displayPath(e.path) + "]" + gone));
    }
    return 0;
  }

  std::string rel;
  CHR_CLI_TRY(toRepoRel(ctx.repo, ap.positional[0], &rel));
  std::vector<int> ids = ctx.hist.forPrefix(rel);
  std::vector<const HistEvent*> evs = ctx.sorted(ids);
  if (evs.empty()) return report(Err("no history for '" + rel + "'"));
  if (first) {
    for (auto it = evs.rbegin(); it != evs.rend(); ++it)
      if ((*it)->kind == 'A') {
        CHR_CLI_TRY(ctx.line(**it, "  [" + displayPath((*it)->path) + "]"));
        std::printf("%s\n", (*it)->commit.hex().c_str());
        return 0;
      }
    return report(Err("'" + rel + "' was never added"));
  }
  for (const HistEvent* e : evs) {
    std::string extra = e->path == rel ? "" : "  [" + displayPath(e->path) + "]";
    CHR_CLI_TRY(ctx.line(*e, extra));
  }
  return 0;
}

}
}
