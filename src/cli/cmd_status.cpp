#include <cstdio>

#include "cli/common.hpp"
#include "engine/worktree.hpp"
#include "store/state.hpp"

namespace chr {
namespace cli {

static const char* kindWord(char k) {
  switch (k) {
    case 'A': return "new file:";
    case 'D': return "deleted: ";
    default: return "modified:";
  }
}

int cmdStatus(const std::vector<std::string>& a) {
  ArgParser ap(a);
  CHR_CLI_TRY(ap.finish());
  Repo repo;
  CHR_CLI_TRY(openRepo(&repo));
  Index index;
  CHR_CLI_TRY(index.load(repo.path("index")));
  HeadInfo head;
  CHR_CLI_TRY(repo.refs.readHead(&head));
  if (head.detached)
    std::printf("HEAD detached at %s\n", head.id.shortHex().c_str());
  else
    std::printf("On branch %s%s\n", head.branch.c_str(), head.unborn ? " (no commits yet)" : "");
  MergeState ms;
  CHR_CLI_TRY(ms.load(repo.dir));
  if (ms.active) {
    std::printf("%s %s", ms.kind == "merge" ? "Merging" : "Cherry-picking", ms.branch.c_str());
    if (ms.conflicts.empty()) {
      std::printf("; all conflicts resolved (run 'chronicle commit')\n");
    } else {
      std::printf("; unresolved conflicts:\n");
      for (const std::string& c : ms.conflicts) std::printf("  conflict:   %s\n", displayPath(c).c_str());
    }
  }
  StatusReport st;
  CHR_CLI_TRY(computeStatus(repo, index, &st));
  if (!st.staged.empty()) {
    std::printf("Staged:\n");
    for (const auto& c : st.staged) std::printf("  %s   %s\n", kindWord(c.first), displayPath(c.second).c_str());
  }
  if (!st.unstaged.empty()) {
    std::printf("Not staged:\n");
    for (const auto& c : st.unstaged) std::printf("  %s   %s\n", kindWord(c.first), displayPath(c.second).c_str());
  }
  if (!st.untracked.empty()) {
    std::printf("Untracked:\n");
    for (const std::string& u : st.untracked) std::printf("  %s\n", displayPath(u).c_str());
  }
  if (st.clean() && st.untracked.empty()) std::printf("nothing to commit, working tree clean\n");
  return 0;
}

}
}
