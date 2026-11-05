#pragma once
#include <string>

#include "ds/trie.hpp"
#include "store/repo.hpp"

namespace chr {

Status buildIdTrie(ObjectStore& store, HexTrie* trie);

Status resolveRevision(Repo& repo, const std::string& spec, Id* out);
Status resolveAnyObject(Repo& repo, const std::string& spec, Id* out);

std::string shortIdFor(const HexTrie& trie, const Id& id, size_t minLen);

}
