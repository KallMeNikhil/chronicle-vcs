#include "exp/patchid.hpp"

#include "core/sha256.hpp"
#include "core/strutil.hpp"
#include "engine/diff.hpp"
#include "engine/treediff.hpp"

namespace chr {

Status patchId(ObjectStore& store, const Id& commit, Id* out) {
  Commit c;
  CHR_TRY(store.readCommit(commit, &c));
  Id parentTree;
  if (!c.parents.empty()) CHR_TRY(commitTreeId(store, c.parents[0], &parentTree));
  std::vector<TreeChange> changes;
  CHR_TRY(diffTrees(store, c.parents.empty() ? nullptr : &parentTree, &c.tree, &changes));
  Sha256 h;
  for (const TreeChange& ch : changes) {
    std::string a, b;
    if (ch.kind != 'A') CHR_TRY(store.readBlob(ch.oldE.id, &a));
    if (ch.kind != 'D') CHR_TRY(store.readBlob(ch.newE.id, &b));
    h.update("file " + ch.path + "\n");
    if (isBinary(a) || isBinary(b)) {
      h.update("binary " + ch.oldE.id.hex() + " " + ch.newE.id.hex() + "\n");
      continue;
    }
    std::vector<std::string> la = splitLines(a), lb = splitLines(b);
    for (const DiffOp& op : diffLines(la, lb)) {
      if (op.kind == '-') h.update("-" + trim(la[static_cast<size_t>(op.a)]) + "\n");
      if (op.kind == '+') h.update("+" + trim(lb[static_cast<size_t>(op.b)]) + "\n");
    }
  }
  *out = h.finish();
  return Status::Ok();
}

}
