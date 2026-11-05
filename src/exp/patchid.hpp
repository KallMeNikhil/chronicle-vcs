#pragma once
#include "store/objstore.hpp"

namespace chr {

Status patchId(ObjectStore& store, const Id& commit, Id* out);

}
