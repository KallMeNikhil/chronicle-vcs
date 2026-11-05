#pragma once
#include <string>
#include <vector>

#include "engine/treediff.hpp"
#include "store/repo.hpp"

namespace chr {
namespace cli {

Status renderChange(Repo& repo, const TreeChange& c, bool newFromWork, std::string* out);
Status printChanges(Repo& repo, const std::vector<TreeChange>& changes, bool newFromWork,
                    const std::vector<std::string>& filters);
bool pathMatches(const std::string& path, const std::vector<std::string>& filters);

}
}
