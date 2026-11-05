#pragma once
#include <string>
#include <vector>

#include "core/status.hpp"

namespace chr {

struct ProcResult {
  bool exited = false;
  int code = 0;
};

Status runProcess(const std::string& dir, const std::vector<std::string>& argv, ProcResult* out);

}
