#include "cli/diffprint.hpp"

#include <cstdio>

#include "core/strutil.hpp"
#include "engine/diff.hpp"
#include "util/fs.hpp"

namespace chr {
namespace cli {

bool pathMatches(const std::string& path, const std::vector<std::string>& filters) {
  if (filters.empty()) return true;
  for (const std::string& f : filters)
    if (f.empty() || path == f || startsWith(path, f + "/")) return true;
  return false;
}

static Status contentOf(Repo& repo, const std::string& path, const FlatEntry& e, bool fromWork,
                        std::string* out) {
  if (!fromWork) return repo.store.readBlob(e.id, out);
  std::string abs = repo.work + "/" + path;
  if (e.mode == kModeLink) return fsu::readLink(abs, out);
  return fsu::readFile(abs, out);
}

Status renderChange(Repo& repo, const TreeChange& c, bool newFromWork, std::string* out) {
  std::string p = displayPath(c.path);
  std::string a, b;
  if (c.kind != 'A') CHR_TRY(contentOf(repo, c.path, c.oldE, false, &a));
  if (c.kind != 'D') CHR_TRY(contentOf(repo, c.path, c.newE, newFromWork, &b));
  *out = "diff --chronicle a/" + p + " b/" + p + "\n";
  if (c.kind == 'A') *out += "new file mode " + modeString(c.newE.mode) + "\n";
  if (c.kind == 'D') *out += "deleted file mode " + modeString(c.oldE.mode) + "\n";
  if (c.kind == 'M' && c.oldE.mode != c.newE.mode)
    *out += "old mode " + modeString(c.oldE.mode) + "\nnew mode " + modeString(c.newE.mode) + "\n";
  if (a == b && c.kind == 'M') return Status::Ok();
  if (isBinary(a) || isBinary(b)) {
    *out += "Binary files differ\n";
    return Status::Ok();
  }
  *out += c.kind == 'A' ? "--- /dev/null\n" : "--- a/" + p + "\n";
  *out += c.kind == 'D' ? "+++ /dev/null\n" : "+++ b/" + p + "\n";
  *out += unifiedHunks(a, b);
  return Status::Ok();
}

Status printChanges(Repo& repo, const std::vector<TreeChange>& changes, bool newFromWork,
                    const std::vector<std::string>& filters) {
  for (const TreeChange& c : changes) {
    if (!pathMatches(c.path, filters)) continue;
    std::string text;
    CHR_TRY(renderChange(repo, c, newFromWork, &text));
    std::fwrite(text.data(), 1, text.size(), stdout);
  }
  return Status::Ok();
}

}
}
