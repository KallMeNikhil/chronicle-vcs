#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#include "cli/cli.hpp"
#include "store/object.hpp"
#include "store/index.hpp"
#include "store/repo.hpp"

#define CHR_CLI_TRY(expr)                              \
  do {                                                 \
    ::chr::Status chr_cs_ = (expr);                    \
    if (!chr_cs_.ok()) return ::chr::cli::report(chr_cs_); \
  } while (0)

namespace chr {
class Dag;
namespace cli {

Status resolveObject(Repo& repo, const std::string& name, Id* out);
void printTreeEntries(const std::vector<TreeEntry>& entries, const std::string& prefix);
Status toRepoRel(const Repo& repo, const std::string& arg, std::string* rel);
Status openRepo(Repo* repo);
Status decorations(Repo& repo, std::unordered_map<Id, std::string, IdHash>* out);
Status printCommitHeader(const Id& id, const Commit& c);

Status resetHardTo(Repo& repo, const Id& target);
Status runThreeWay(Repo& repo, Index& index, const Id* base, const Id& oursCommit, const Id& theirsCommit,
                   const std::string& oursLabel, const std::string& theirsLabel, const std::string& kind,
                   const std::string& message, const std::vector<Id>& parents, Id* made);

struct LogOptions {
  bool oneline = false;
  bool graph = false;
  int64_t limit = -1;
};
Status printLog(Repo& repo, const Dag& dag, const std::vector<int>& order, const LogOptions& opt);

}
}
