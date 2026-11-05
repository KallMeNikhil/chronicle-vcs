#include "engine/commitops.hpp"

#include <cstdlib>

#include "core/strutil.hpp"
#include "core/timeutil.hpp"
#include "engine/history.hpp"

namespace chr {

static Status authorTime(int64_t* t, int* tz) {
  const char* env = std::getenv("CHRONICLE_AUTHOR_DATE");
  if (env && *env) {
    std::vector<std::string> parts = split(trim(env), ' ');
    if (!parseInt64(parts[0], t)) return UsageErr("CHRONICLE_AUTHOR_DATE must be '<unix seconds> [+hhmm]'");
    *tz = 0;
    if (parts.size() > 1 && !parseTz(parts[1], tz))
      return UsageErr("CHRONICLE_AUTHOR_DATE has a bad timezone (use +hhmm)");
    return Status::Ok();
  }
  *t = nowSeconds();
  *tz = localTzOffsetMinutes(*t);
  return Status::Ok();
}

Status createCommit(Repo& repo, const Id& tree, const std::vector<Id>& parents, const std::string& message,
                    Id* out) {
  Commit c;
  c.tree = tree;
  c.parents = parents;
  c.author = repo.authorIdent();
  CHR_TRY(authorTime(&c.time, &c.tz));
  c.message = message;
  if (c.message.empty() || c.message.back() != '\n') c.message += '\n';
  return repo.store.put(ObjType::Commit, encodeCommit(c), out);
}

Status recordCommit(Repo& repo, const Id& commit) {
  CHR_TRY(repo.refs.advanceHead(commit));
  return appendHistory(repo, commit);
}

}
