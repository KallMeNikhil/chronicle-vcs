#include <cstdio>

#include "cli/common.hpp"
#include "engine/fsck.hpp"

namespace chr {
namespace cli {

int cmdVerify(const std::vector<std::string>& a) {
  ArgParser ap(a);
  CHR_CLI_TRY(ap.finish());
  if (!ap.positional.empty()) return report(UsageErr("usage: chronicle verify"));
  Repo repo;
  CHR_CLI_TRY(openRepo(&repo));
  FsckReport r;
  CHR_CLI_TRY(runFsck(repo, &r));
  for (const std::string& c : r.corrupt) std::printf("corrupt: %s\n", c.c_str());
  for (const auto& m : r.missing)
    std::printf("missing: %s (referenced by %s)\n", m.first.hex().c_str(), m.second.hex().c_str());
  for (const auto& d : r.dangling) std::printf("dangling %s %s\n", typeName(d.first), d.second.hex().c_str());
  std::printf("checked %zu objects: %zu corrupt, %zu missing, %zu dangling\n", r.checked, r.corrupt.size(),
              r.missing.size(), r.dangling.size());
  if (!r.ok()) return report(Err("repository is damaged"));
  return 0;
}

}
}
