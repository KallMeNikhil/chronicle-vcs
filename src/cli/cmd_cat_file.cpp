#include <cstdio>

#include "cli/cli.hpp"
#include "cli/common.hpp"
#include "store/repo.hpp"

namespace chr {
namespace cli {

int cmdCatFile(const std::vector<std::string>& a) {
  ArgParser ap(a);
  bool showType = ap.flag("-t"), showSize = ap.flag("-s"), pretty = ap.flag("-p");
  Status s = ap.finish();
  if (!s.ok()) return report(s);
  if (ap.positional.size() != 1 || (showType + showSize + pretty) != 1)
    return report(UsageErr("usage: chronicle cat-file -t|-s|-p <id>"));
  Repo repo;
  CHR_CLI_TRY(repo.open());
  Id id;
  CHR_CLI_TRY(resolveObject(repo, ap.positional[0], &id));
  ObjType t;
  std::string payload;
  CHR_CLI_TRY(repo.store.get(id, &t, &payload));
  if (showType) {
    std::printf("%s\n", typeName(t));
  } else if (showSize) {
    std::printf("%zu\n", payload.size());
  } else if (t == ObjType::Tree) {
    std::vector<TreeEntry> entries;
    CHR_CLI_TRY(decodeTree(payload, &entries));
    printTreeEntries(entries, "");
  } else {
    std::fwrite(payload.data(), 1, payload.size(), stdout);
  }
  return 0;
}

}
}
