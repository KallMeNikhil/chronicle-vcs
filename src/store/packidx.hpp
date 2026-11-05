#pragma once
#include <string>
#include <vector>

#include "core/status.hpp"
#include "ds/radixsort.hpp"

namespace chr {

std::string encodePackIndex(std::vector<IdEntry>& entries);
Status decodePackIndex(const std::string& bytes, std::vector<IdEntry>* out);

}
