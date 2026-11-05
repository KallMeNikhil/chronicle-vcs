#include <cstdio>

#include "cli/cli.hpp"
#include "store/repo.hpp"

namespace chr {
namespace cli {

int cmdInit(const std::vector<std::string>& a) {
  ArgParser ap(a);
  Status s = ap.finish();
  if (!s.ok()) return report(s);
  if (ap.positional.size() > 1) return report(UsageErr("usage: chronicle init [dir]"));
  std::string created;
  s = Repo::init(ap.positional.empty() ? "." : ap.positional[0], &created);
  if (!s.ok()) return report(s);
  std::printf("Initialized empty Chronicle repository in %s\n", created.c_str());
  return 0;
}

}
}
