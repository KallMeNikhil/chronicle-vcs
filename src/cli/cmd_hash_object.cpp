#include <cstdio>

#include "cli/cli.hpp"
#include "store/repo.hpp"
#include "util/fs.hpp"

namespace chr {
namespace cli {

int cmdHashObject(const std::vector<std::string>& a) {
  ArgParser ap(a);
  bool write = ap.flag("-w");
  Status s = ap.finish();
  if (!s.ok()) return report(s);
  if (ap.positional.size() != 1) return report(UsageErr("usage: chronicle hash-object [-w] <file>"));
  std::string data;
  s = fsu::readFile(ap.positional[0], &data);
  if (!s.ok()) return report(s);
  Id id = objectId(ObjType::Blob, data);
  if (write) {
    Repo repo;
    s = repo.open();
    if (s.ok()) s = repo.store.put(ObjType::Blob, data, &id);
    if (!s.ok()) return report(s);
  }
  std::printf("%s\n", id.hex().c_str());
  return 0;
}

}
}
