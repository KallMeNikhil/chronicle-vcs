#pragma once
#include <string>
#include <vector>

#include "store/repo.hpp"

namespace chr {

Status createCommit(Repo& repo, const Id& tree, const std::vector<Id>& parents, const std::string& message,
                    Id* out);
Status recordCommit(Repo& repo, const Id& commit);

}
