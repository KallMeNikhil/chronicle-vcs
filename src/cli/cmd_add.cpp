#include <cstdio>

#include "cli/common.hpp"
#include "engine/worktree.hpp"
#include "store/lock.hpp"
#include "store/state.hpp"

namespace chr {
namespace cli {

namespace {

struct Adder {
  Repo& repo;
  Index& index;
  MergeState& ms;
  std::vector<std::string> problems;

  void stage(const std::string& rel, const fsu::FileStat& st) {
    auto old = index.entries.find(rel);
    if (old != index.entries.end() && statClean(old->second, st, index.writeTimeNs)) {
      ms.markResolved(rel);
      return;
    }
    if (st.isReg && static_cast<int64_t>(st.size) > repo.maxFileBytes()) {
      problems.push_back("file too large: '" + rel + "' is " + std::to_string(st.size) +
                         " bytes (core.maxfilebytes = " + std::to_string(repo.maxFileBytes()) + ")");
      return;
    }
    IndexEntry e;
    Status s = hashWorkFile(repo, rel, st, true, &e);
    if (!s.ok()) {
      problems.push_back("skipped '" + rel + "': " + s.msg);
      return;
    }
    index.eraseTree(rel);
    for (size_t p = rel.find('/'); p != std::string::npos; p = rel.find('/', p + 1))
      index.entries.erase(rel.substr(0, p));
    index.entries[rel] = e;
    ms.markResolved(rel);
  }

  void stageDeletionsUnder(const std::string& rel, const std::map<std::string, fsu::FileStat>& present) {
    std::vector<std::string> gone;
    std::string pre = rel.empty() ? "" : rel + "/";
    for (auto it = index.entries.lower_bound(pre); it != index.entries.end(); ++it) {
      if (!startsWith(it->first, pre)) break;
      if (!present.count(it->first) && !fsu::lstatPath(repo.work + "/" + it->first).exists)
        gone.push_back(it->first);
    }
    for (const std::string& g : gone) {
      index.entries.erase(g);
      ms.markResolved(g);
    }
  }
};

}

int cmdAdd(const std::vector<std::string>& a) {
  ArgParser ap(a);
  CHR_CLI_TRY(ap.finish());
  std::vector<std::string> paths = ap.positional;
  paths.insert(paths.end(), ap.afterDashDash.begin(), ap.afterDashDash.end());
  if (paths.empty()) return report(UsageErr("usage: chronicle add <path>..."));
  Repo repo;
  CHR_CLI_TRY(openRepo(&repo));
  RepoLock lock;
  CHR_CLI_TRY(lock.acquire(repo.dir));
  Index index;
  CHR_CLI_TRY(index.load(repo.path("index")));
  Ignore ignore;
  CHR_CLI_TRY(ignore.load(repo.work));
  MergeState ms;
  CHR_CLI_TRY(ms.load(repo.dir));
  Adder adder{repo, index, ms, {}};

  for (const std::string& arg : paths) {
    std::string rel;
    CHR_CLI_TRY(toRepoRel(repo, arg, &rel));
    fsu::FileStat st = fsu::lstatPath(rel.empty() ? repo.work : repo.work + "/" + rel);
    if (!st.exists) {
      if (index.entries.count(rel) || index.hasPrefix(rel)) {
        index.eraseTree(rel);
        ms.markResolved(rel);
        continue;
      }
      return report(Err("pathspec '" + arg + "' did not match any files"));
    }
    if (st.isDir) {
      if (!rel.empty() && ignore.ignored(rel, true)) {
        std::fprintf(stderr, "chronicle: ignoring '%s' (matches .chronicleignore)\n", rel.c_str());
        continue;
      }
      std::map<std::string, fsu::FileStat> files;
      std::vector<std::string> unreadable;
      CHR_CLI_TRY(walkWorktree(repo, ignore, rel, &files, &unreadable));
      for (const std::string& u : unreadable) adder.problems.push_back("skipped unreadable directory '" + u + "'");
      for (const auto& kv : files) adder.stage(kv.first, kv.second);
      adder.stageDeletionsUnder(rel, files);
    } else if (ignore.ignored(rel, false)) {
      std::fprintf(stderr, "chronicle: ignoring '%s' (matches .chronicleignore)\n", rel.c_str());
    } else {
      adder.stage(rel, st);
    }
  }
  CHR_CLI_TRY(index.save(repo.path("index")));
  if (ms.active) CHR_CLI_TRY(ms.save(repo.dir));
  for (const std::string& p : adder.problems) std::fprintf(stderr, "chronicle: %s\n", p.c_str());
  return adder.problems.empty() ? 0 : kErrGeneral;
}

}
}
