#pragma once
#include <cstdint>
#include <string>

#include "core/status.hpp"

namespace chr {

std::string makeDelta(const std::string& base, const std::string& target);
Status applyDelta(const std::string& base, const std::string& delta, std::string* out);
uint64_t deltaResultSize(const std::string& delta);

}
