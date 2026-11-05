#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/id.hpp"
#include "core/strutil.hpp"

namespace chr {

class HexTrie {
 public:
  enum class Kind { None, Unique, Ambiguous };
  struct Result {
    Kind kind = Kind::None;
    Id id;
    uint32_t count = 0;
    std::vector<Id> candidates;
  };
  static const size_t kMinPrefix = 4;

  HexTrie() { nodes_.push_back(Node()); }

  size_t size() const { return nodes_[0].count; }

  void insert(const Id& id) {
    if (contains(id)) return;
    const std::string h = id.hex();
    int32_t idx = static_cast<int32_t>(ids_.size());
    ids_.push_back(id);
    int32_t n = 0;
    for (size_t d = 0;; ++d) {
      nodes_[n].count++;
      if (nodes_[n].leaf >= 0) {
        int32_t old = nodes_[n].leaf;
        nodes_[n].leaf = -1;
        int c = hexVal(ids_[old].hex()[d]);
        int32_t child = newNode();
        nodes_[child].count = 1;
        nodes_[child].leaf = old;
        nodes_[n].child[c] = child;
      }
      int c = hexVal(h[d]);
      if (nodes_[n].child[c] < 0) {
        int32_t child = newNode();
        nodes_[child].count = 1;
        nodes_[child].leaf = idx;
        nodes_[n].child[c] = child;
        return;
      }
      n = nodes_[n].child[c];
    }
  }

  bool contains(const Id& id) const {
    Result r = resolve(id.hex());
    return r.kind == Kind::Unique && r.id == id;
  }

  Result resolve(const std::string& prefix) const {
    Result r;
    if (prefix.size() > 64 || (!prefix.empty() && !isLowerHex(prefix))) return r;
    int32_t n = 0;
    for (size_t d = 0; d < prefix.size(); ++d) {
      if (nodes_[n].leaf >= 0) return bucketMatch(nodes_[n].leaf, prefix);
      int32_t child = nodes_[n].child[hexVal(prefix[d])];
      if (child < 0) return r;
      n = child;
    }
    if (nodes_[n].leaf >= 0) return bucketMatch(nodes_[n].leaf, prefix);
    r.count = nodes_[n].count;
    if (r.count == 0) return r;
    collect(n, &r.candidates, 10);
    if (r.count == 1) {
      r.kind = Kind::Unique;
      r.id = r.candidates[0];
      r.candidates.clear();
    } else {
      r.kind = Kind::Ambiguous;
    }
    return r;
  }

  size_t uniquePrefixLen(const Id& id, size_t minLen) const {
    const std::string h = id.hex();
    int32_t n = 0;
    size_t d = 0;
    while (d < 64 && nodes_[n].leaf < 0) {
      int32_t child = nodes_[n].child[hexVal(h[d])];
      if (child < 0) break;
      n = child;
      ++d;
    }
    return d > minLen ? d : minLen;
  }

  bool checkInvariants() const {
    uint32_t total = 0;
    if (!checkNode(0, 0, &total)) return false;
    if (total != nodes_[0].count || total != ids_.size()) return false;
    for (const Id& id : ids_)
      if (!contains(id)) return false;
    return true;
  }

 private:
  struct Node {
    int32_t child[16];
    uint32_t count = 0;
    int32_t leaf = -1;
    Node() {
      for (int32_t& c : child) c = -1;
    }
  };

  int32_t newNode() {
    nodes_.push_back(Node());
    return static_cast<int32_t>(nodes_.size() - 1);
  }

  Result bucketMatch(int32_t leaf, const std::string& prefix) const {
    Result r;
    if (startsWith(ids_[leaf].hex(), prefix)) {
      r.kind = Kind::Unique;
      r.id = ids_[leaf];
      r.count = 1;
    }
    return r;
  }

  void collect(int32_t n, std::vector<Id>* out, size_t limit) const {
    if (out->size() >= limit) return;
    if (nodes_[n].leaf >= 0) {
      out->push_back(ids_[nodes_[n].leaf]);
      return;
    }
    for (int32_t c : nodes_[n].child)
      if (c >= 0) collect(c, out, limit);
  }

  bool checkNode(int32_t n, size_t depth, uint32_t* leaves) const {
    const Node& node = nodes_[n];
    if (node.leaf >= 0) {
      for (int32_t c : node.child)
        if (c >= 0) return false;
      if (node.count != 1) return false;
      ++*leaves;
      return true;
    }
    uint32_t sum = 0;
    for (int32_t c : node.child) {
      if (c < 0) continue;
      uint32_t sub = 0;
      if (!checkNode(c, depth + 1, &sub)) return false;
      sum += sub;
    }
    if (sum != node.count) return false;
    *leaves += sum;
    return true;
  }

  std::vector<Node> nodes_;
  std::vector<Id> ids_;
};

}
