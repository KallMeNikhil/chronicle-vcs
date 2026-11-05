#pragma once
#include <string>
#include <vector>

#include "engine/dag.hpp"

namespace chr {

struct GraphRow {
  std::vector<std::string> before;
  std::string prefix;
  std::vector<std::string> after;
  std::string continuation;
};

class GraphLayout {
 public:
  explicit GraphLayout(const Dag& dag) : dag_(dag) {}
  GraphRow next(int commit);

 private:
  std::string laneRow(int star) const;
  const Dag& dag_;
  std::vector<int> lanes_;
};

std::string dotGraph(const Dag& dag, const std::vector<int>& order,
                     const std::vector<std::pair<std::string, int>>& branchTips,
                     const std::vector<std::string>& summaries);

}
