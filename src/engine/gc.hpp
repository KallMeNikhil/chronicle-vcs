#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "store/pack.hpp"
#include "store/repo.hpp"

namespace chr {

using IdSet = std::unordered_set<Id, IdHash>;

Status gcRoots(Repo& repo, std::vector<Id>* roots);
Status markReachable(ObjectStore& store, const std::vector<Id>& roots, IdSet* reach,
                     std::vector<std::pair<Id, Id>>* missing, bool skipUnreadable = false);

Status packLoose(Repo& repo, bool keepLoose, PackWriteStats* stats);

struct GcReport {
  size_t reachable = 0;
  size_t looseDeleted = 0;
  size_t looseKeptGrace = 0;
  size_t looseRepacked = 0;
  size_t packedDropped = 0;
  size_t oldPacks = 0;
  PackWriteStats pack;
};
Status runGc(Repo& repo, bool dryRun, int64_t graceSeconds, GcReport* out);

}
