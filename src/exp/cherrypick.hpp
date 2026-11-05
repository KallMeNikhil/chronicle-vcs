#pragma once
#include "store/repo.hpp"

namespace chr {

Status findAppliedEquivalent(Repo& repo, const Id& rev, const Id& head, bool* found, Id* as);

}
