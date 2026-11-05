#include <cstdio>

#include "cli/common.hpp"
#include "engine/bisect.hpp"
#include "engine/revparse.hpp"
#include "engine/timetravel.hpp"
#include "store/lock.hpp"
#include "util/fs.hpp"
#include "util/proc.hpp"

namespace chr {
namespace cli {

namespace {

Status prepareWork(Repo& repo, const Id& commit, std::string* dir) {
  *dir = repo.path("bisect-work");
  CHR_TRY(fsu::removeTree(*dir));
  size_t files = 0;
  return exportTree(repo, commit, *dir, &files);
}

Status advance(Repo& repo, BisectState& st, bool quiet, BisectStep* step, bool* finished) {
  *finished = false;
  if (!st.haveBad || st.goods.empty()) {
    std::printf("bisect: waiting for %s\n", st.haveBad ? "a good commit" : "a bad commit");
    return st.save(repo.dir);
  }
  CHR_TRY(bisectNext(repo.store, st, step));
  if (!step->warning.empty()) std::fprintf(stderr, "warning: %s\n", step->warning.c_str());
  if (step->done) {
    *finished = true;
    st.haveNext = false;
    st.log.push_back("first bad commit " + step->firstBad.hex());
    CHR_TRY(st.save(repo.dir));
    Commit c;
    CHR_TRY(repo.store.readCommit(step->firstBad, &c));
    std::printf("first bad commit: %s\n", step->firstBad.hex().c_str());
    std::printf("    %s\n", c.summary().c_str());
    return Status::Ok();
  }
  if (step->onlySkipped) {
    *finished = true;
    std::printf("only skipped commits left to test; the first bad commit is one of:\n");
    for (const Id& id : step->left) std::printf("  %s\n", id.hex().c_str());
    return st.save(repo.dir);
  }
  st.haveNext = true;
  st.next = step->test;
  CHR_TRY(st.save(repo.dir));
  if (!quiet) {
    std::string dir;
    CHR_TRY(prepareWork(repo, step->test, &dir));
    std::printf("Bisecting: %zu candidates left; test %s (exported to .chronicle/bisect-work)\n", step->remaining,
                step->test.shortHex().c_str());
  }
  return Status::Ok();
}

Status target(Repo& repo, const BisectState& st, const std::vector<std::string>& args, Id* out) {
  if (args.size() > 1) return UsageErr("too many arguments");
  if (!args.empty()) return resolveRevision(repo, args[0], out);
  if (st.haveNext) {
    *out = st.next;
    return Status::Ok();
  }
  return resolveRevision(repo, "HEAD", out);
}

Status mark(Repo& repo, BisectState& st, const std::string& what, const Id& id) {
  if (what == "bad") {
    st.bad = id;
    st.haveBad = true;
  } else if (what == "good") {
    st.goods.push_back(id);
  } else {
    st.skips.push_back(id);
  }
  st.log.push_back(what + " " + id.hex());
  (void)repo;
  return Status::Ok();
}

int runLoop(Repo& repo, BisectState& st, const std::vector<std::string>& cmd) {
  int tests = 0;
  for (;;) {
    BisectStep step;
    bool finished = false;
    CHR_CLI_TRY(advance(repo, st, true, &step, &finished));
    if (finished) break;
    if (!st.haveBad || st.goods.empty()) return report(Err("bisect run needs a bad and a good commit first"));
    std::string dir;
    CHR_CLI_TRY(prepareWork(repo, step.test, &dir));
    ProcResult r;
    CHR_CLI_TRY(runProcess(dir, cmd, &r));
    ++tests;
    if (!r.exited || r.code >= 128) {
      st.log.push_back("run aborted at " + step.test.hex());
      CHR_CLI_TRY(st.save(repo.dir));
      return report(Err("bisect run: test command " +
                        std::string(r.exited ? "exited with " + std::to_string(r.code) : "was killed by signal " +
                                                                                           std::to_string(r.code)) +
                        " at " + step.test.shortHex() + "; aborting"));
    }
    std::string verdict = r.code == 0 ? "good" : (r.code == 125 ? "skip" : "bad");
    std::printf("step %d: %s is %s (%zu candidates)\n", tests, step.test.shortHex().c_str(), verdict.c_str(),
                step.remaining);
    CHR_CLI_TRY(mark(repo, st, verdict, step.test));
  }
  fsu::removeTree(repo.path("bisect-work"));
  std::printf("tests run: %d\n", tests);
  return 0;
}

}

int cmdBisect(const std::vector<std::string>& a) {
  if (a.empty()) return report(UsageErr("usage: chronicle bisect start|bad|good|skip|run|reset|log"));
  const std::string sub = a[0];
  std::vector<std::string> rest(a.begin() + 1, a.end());
  Repo repo;
  CHR_CLI_TRY(openRepo(&repo));
  RepoLock lock;
  CHR_CLI_TRY(lock.acquire(repo.dir));
  BisectState st;
  CHR_CLI_TRY(st.load(repo.dir));

  if (sub == "start") {
    std::string strategy = "linear";
    for (const std::string& r : rest) {
      if (r == "--strategy=weighted" || r == "--strategy=linear") strategy = r.substr(11);
      else return report(UsageErr("usage: chronicle bisect start [--strategy=linear|weighted]"));
    }
    st = BisectState();
    st.active = true;
    st.strategy = strategy;
    CHR_CLI_TRY(fsu::readFile(repo.path("HEAD"), &st.orig));
    st.log.push_back("start " + strategy);
    CHR_CLI_TRY(st.save(repo.dir));
    std::printf("bisect started (%s strategy)\n", strategy.c_str());
    return 0;
  }
  if (!st.active) return report(Err("no bisect in progress (run 'chronicle bisect start')"));
  if (sub == "reset") {
    CHR_CLI_TRY(fsu::removeFile(repo.path("BISECT")));
    CHR_CLI_TRY(fsu::removeTree(repo.path("bisect-work")));
    std::printf("bisect reset\n");
    return 0;
  }
  if (sub == "log") {
    for (const std::string& l : st.log) std::printf("%s\n", l.c_str());
    return 0;
  }
  if (sub == "bad" || sub == "good" || sub == "skip") {
    Id id;
    CHR_CLI_TRY(target(repo, st, rest, &id));
    CHR_CLI_TRY(mark(repo, st, sub, id));
    BisectStep step;
    bool finished = false;
    CHR_CLI_TRY(advance(repo, st, false, &step, &finished));
    return 0;
  }
  if (sub == "run") {
    if (rest.empty()) return report(UsageErr("usage: chronicle bisect run <cmd...>"));
    return runLoop(repo, st, rest);
  }
  return report(UsageErr("unknown bisect subcommand '" + sub + "'"));
}

}
}
