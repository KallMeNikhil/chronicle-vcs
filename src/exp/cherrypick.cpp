#include "exp/cherrypick.hpp"

#include "engine/dag.hpp"
#include "exp/patchid.hpp"

namespace chr {

Status findAppliedEquivalent(Repo& repo, const Id& rev, const Id& head, bool* found, Id* as) {
  *found = false;
  Dag dag;
  CHR_TRY(dag.load(repo.store, {head, rev}));
  int h = dag.idxOf(head), r = dag.idxOf(rev);
  if (dag.isAncestor(r, h)) {
    *found = true;
    *as = rev;
    return Status::Ok();
  }
  std::vector<char> inHead = dag.reach(h);
  std::vector<int> bases = dag.mergeBases(h, r);
  std::vector<char> inBase(static_cast<size_t>(dag.size()), 0);
  for (int b : bases) {
    std::vector<char> rb = dag.reach(b);
    for (size_t i = 0; i < rb.size(); ++i)
      if (rb[i]) inBase[i] = 1;
  }
  Id want;
  CHR_TRY(patchId(repo.store, rev, &want));
  for (int i = 0; i < dag.size(); ++i) {
    if (!inHead[static_cast<size_t>(i)] || inBase[static_cast<size_t>(i)]) continue;
    if (dag.node(i).parents.size() > 1) continue;
    Id pid;
    CHR_TRY(patchId(repo.store, dag.node(i).id, &pid));
    if (pid == want) {
      *found = true;
      *as = dag.node(i).id;
      return Status::Ok();
    }
  }
  return Status::Ok();
}

}
