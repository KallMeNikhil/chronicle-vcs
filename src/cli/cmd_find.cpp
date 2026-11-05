#include <cstdio>

#include "cli/common.hpp"
#include "engine/revparse.hpp"

namespace chr {
namespace cli {

static const char* typeOf(Repo& repo, const Id& id) {
  ObjType t = ObjType::None;
  std::string payload;
  if (!repo.store.get(id, &t, &payload).ok()) return "corrupt";
  return typeName(t);
}

int cmdFind(const std::vector<std::string>& a) {
  ArgParser ap(a);
  CHR_CLI_TRY(ap.finish());
  if (ap.positional.size() != 1) return report(UsageErr("usage: chronicle find <prefix>"));
  const std::string& prefix = ap.positional[0];
  if (!isLowerHex(prefix) || prefix.size() < HexTrie::kMinPrefix || prefix.size() > 64)
    return report(UsageErr("prefix must be 4..64 lowercase hex characters"));
  Repo repo;
  CHR_CLI_TRY(openRepo(&repo));
  HexTrie trie;
  CHR_CLI_TRY(buildIdTrie(repo.store, &trie));
  HexTrie::Result r = trie.resolve(prefix);
  if (r.kind == HexTrie::Kind::None) return report(Err("no object matches '" + prefix + "'"));
  if (r.kind == HexTrie::Kind::Unique) {
    std::printf("%s %s\n", r.id.hex().c_str(), typeOf(repo, r.id));
    return 0;
  }
  for (const Id& c : r.candidates) std::printf("%s %s\n", c.hex().c_str(), typeOf(repo, c));
  return report(Err("ambiguous prefix '" + prefix + "' (" + std::to_string(r.count) + " objects)"));
}

}
}
