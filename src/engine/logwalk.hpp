#pragma once
#include <vector>

#include "engine/dag.hpp"

namespace chr {

std::vector<int> dateOrderWalk(const Dag& dag, const std::vector<int>& tips);

std::vector<int> sortedWalk(const Dag& dag, const std::vector<int>& tips);

}
