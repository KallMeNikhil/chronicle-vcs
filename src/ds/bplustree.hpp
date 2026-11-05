#pragma once
#include <algorithm>
#include <utility>
#include <vector>

namespace chr {

template <typename K, typename V>
class BPlusTree {
 public:
  struct Iter {
    int leaf = -1;
    int pos = 0;
    bool valid() const { return leaf >= 0; }
  };

  explicit BPlusTree(int order = 64) : m_(order < 3 ? 3 : order) { clear(); }

  void clear() {
    pool_.clear();
    pool_.push_back(Node{true, {}, {}, {}, -1, -1});
    root_ = 0;
    size_ = 0;
  }
  size_t size() const { return size_; }
  int order() const { return m_; }

  void insert(const K& key, const V& val) {
    std::vector<std::pair<int, int>> path;
    int n = root_;
    while (!pool_[n].leaf) {
      int slot = childSlot(n, key);
      path.push_back({n, slot});
      n = pool_[n].kids[static_cast<size_t>(slot)];
    }
    Node& leaf = pool_[n];
    auto it = std::lower_bound(leaf.keys.begin(), leaf.keys.end(), key);
    size_t pos = static_cast<size_t>(it - leaf.keys.begin());
    if (it != leaf.keys.end() && !(key < *it)) {
      leaf.vals[pos] = val;
      return;
    }
    leaf.keys.insert(it, key);
    leaf.vals.insert(leaf.vals.begin() + static_cast<long>(pos), val);
    ++size_;
    if (static_cast<int>(pool_[n].keys.size()) <= m_) return;
    K up;
    int right = splitLeaf(n, &up);
    while (!path.empty()) {
      int parent = path.back().first;
      int slot = path.back().second;
      path.pop_back();
      Node& p = pool_[parent];
      p.keys.insert(p.keys.begin() + slot, up);
      p.kids.insert(p.kids.begin() + slot + 1, right);
      if (static_cast<int>(p.keys.size()) <= m_) return;
      right = splitInternal(parent, &up);
    }
    Node r{false, {up}, {}, {root_, right}, -1, -1};
    pool_.push_back(r);
    root_ = static_cast<int>(pool_.size() - 1);
  }

  void bulkLoad(const std::vector<std::pair<K, V>>& sorted, double fill = 0.7) {
    clear();
    if (sorted.empty()) return;
    pool_.clear();
    std::vector<int> level;
    std::vector<K> mins;
    std::vector<size_t> sizes = distribute(sorted.size(), m_, fill);
    size_t at = 0;
    for (size_t s : sizes) {
      Node leaf{true, {}, {}, {}, -1, -1};
      for (size_t i = 0; i < s; ++i, ++at) {
        leaf.keys.push_back(sorted[at].first);
        leaf.vals.push_back(sorted[at].second);
      }
      int id = static_cast<int>(pool_.size());
      if (!level.empty()) {
        leaf.prev = level.back();
        pool_[static_cast<size_t>(level.back())].next = id;
      }
      mins.push_back(leaf.keys.front());
      pool_.push_back(leaf);
      level.push_back(id);
    }
    size_ = sorted.size();
    while (level.size() > 1) {
      std::vector<size_t> groups = distribute(level.size(), m_ + 1, fill);
      std::vector<int> up;
      std::vector<K> upMins;
      size_t k = 0;
      for (size_t g : groups) {
        Node in{false, {}, {}, {}, -1, -1};
        upMins.push_back(mins[k]);
        for (size_t i = 0; i < g; ++i, ++k) {
          if (i > 0) in.keys.push_back(mins[k]);
          in.kids.push_back(level[k]);
        }
        up.push_back(static_cast<int>(pool_.size()));
        pool_.push_back(in);
      }
      level.swap(up);
      mins.swap(upMins);
    }
    root_ = level[0];
  }

  Iter lowerBound(const K& k) const {
    int n = descend(k);
    const Node& leaf = pool_[static_cast<size_t>(n)];
    Iter it{n, static_cast<int>(std::lower_bound(leaf.keys.begin(), leaf.keys.end(), k) - leaf.keys.begin())};
    normalize(it);
    return it;
  }

  Iter floor(const K& k) const {
    int n = descend(k);
    const Node& leaf = pool_[static_cast<size_t>(n)];
    int pos = static_cast<int>(std::upper_bound(leaf.keys.begin(), leaf.keys.end(), k) - leaf.keys.begin()) - 1;
    while (pos < 0) {
      n = pool_[static_cast<size_t>(n)].prev;
      if (n < 0) return Iter{-1, 0};
      pos = static_cast<int>(pool_[static_cast<size_t>(n)].keys.size()) - 1;
    }
    return Iter{n, pos};
  }

  const K& key(const Iter& it) const { return pool_[static_cast<size_t>(it.leaf)].keys[static_cast<size_t>(it.pos)]; }
  const V& value(const Iter& it) const { return pool_[static_cast<size_t>(it.leaf)].vals[static_cast<size_t>(it.pos)]; }
  void next(Iter& it) const {
    ++it.pos;
    normalize(it);
  }

  std::vector<std::pair<K, V>> rangeScan(const K& lo, const K& hi) const {
    std::vector<std::pair<K, V>> out;
    for (Iter it = lowerBound(lo); it.valid() && !(hi < key(it)); next(it)) out.push_back({key(it), value(it)});
    return out;
  }

  int height() const {
    int h = 1;
    for (int n = root_; !pool_[static_cast<size_t>(n)].leaf; n = pool_[static_cast<size_t>(n)].kids[0]) ++h;
    return h;
  }

  bool checkInvariants() const {
    int leafDepth = -1;
    size_t count = 0;
    if (!checkNode(root_, 1, nullptr, nullptr, &leafDepth, &count)) return false;
    if (count != size_) return false;
    int n = root_;
    while (!pool_[static_cast<size_t>(n)].leaf) n = pool_[static_cast<size_t>(n)].kids[0];
    if (pool_[static_cast<size_t>(n)].prev != -1) return false;
    size_t seen = 0;
    const K* last = nullptr;
    for (int prev = -1; n >= 0; prev = n, n = pool_[static_cast<size_t>(n)].next) {
      if (pool_[static_cast<size_t>(n)].prev != prev) return false;
      for (const K& k : pool_[static_cast<size_t>(n)].keys) {
        if (last && !(*last < k)) return false;
        last = &k;
        ++seen;
      }
    }
    return seen == size_;
  }

 private:
  struct Node {
    bool leaf;
    std::vector<K> keys;
    std::vector<V> vals;
    std::vector<int> kids;
    int next;
    int prev;
  };

  static std::vector<size_t> distribute(size_t total, int cap, double fill) {
    size_t c = static_cast<size_t>(cap);
    size_t minFill = (c + 1) / 2;
    size_t target = std::max(minFill, std::min(c, static_cast<size_t>(static_cast<double>(c) * fill)));
    size_t groups = std::max<size_t>({1, total / target, (total + c - 1) / c});
    std::vector<size_t> out(groups, total / groups);
    for (size_t i = 0; i < total % groups; ++i) ++out[i];
    return out;
  }

  int childSlot(int n, const K& k) const {
    const std::vector<K>& keys = pool_[static_cast<size_t>(n)].keys;
    return static_cast<int>(std::upper_bound(keys.begin(), keys.end(), k) - keys.begin());
  }

  int descend(const K& k) const {
    int n = root_;
    while (!pool_[static_cast<size_t>(n)].leaf) n = pool_[static_cast<size_t>(n)].kids[static_cast<size_t>(childSlot(n, k))];
    return n;
  }

  void normalize(Iter& it) const {
    while (it.leaf >= 0 && it.pos >= static_cast<int>(pool_[static_cast<size_t>(it.leaf)].keys.size())) {
      it.leaf = pool_[static_cast<size_t>(it.leaf)].next;
      it.pos = 0;
    }
  }

  int splitLeaf(int n, K* up) {
    Node right{true, {}, {}, {}, -1, -1};
    {
      Node& left = pool_[static_cast<size_t>(n)];
      size_t total = left.keys.size();
      size_t keep = total - total / 2;
      right.keys.assign(left.keys.begin() + static_cast<long>(keep), left.keys.end());
      right.vals.assign(left.vals.begin() + static_cast<long>(keep), left.vals.end());
      left.keys.resize(keep);
      left.vals.resize(keep);
      right.next = left.next;
      right.prev = n;
    }
    int r = static_cast<int>(pool_.size());
    pool_.push_back(right);
    Node& left = pool_[static_cast<size_t>(n)];
    if (left.next >= 0) pool_[static_cast<size_t>(left.next)].prev = r;
    left.next = r;
    *up = pool_[static_cast<size_t>(r)].keys.front();
    return r;
  }

  int splitInternal(int n, K* up) {
    Node right{false, {}, {}, {}, -1, -1};
    {
      Node& left = pool_[static_cast<size_t>(n)];
      size_t mid = left.keys.size() / 2;
      *up = left.keys[mid];
      right.keys.assign(left.keys.begin() + static_cast<long>(mid) + 1, left.keys.end());
      right.kids.assign(left.kids.begin() + static_cast<long>(mid) + 1, left.kids.end());
      left.keys.resize(mid);
      left.kids.resize(mid + 1);
    }
    pool_.push_back(right);
    return static_cast<int>(pool_.size() - 1);
  }

  bool checkNode(int n, int depth, const K* lo, const K* hi, int* leafDepth, size_t* count) const {
    const Node& node = pool_[static_cast<size_t>(n)];
    bool isRoot = n == root_;
    if (static_cast<int>(node.keys.size()) > m_) return false;
    for (size_t i = 1; i < node.keys.size(); ++i)
      if (!(node.keys[i - 1] < node.keys[i])) return false;
    for (const K& k : node.keys) {
      if (lo && k < *lo) return false;
      if (hi && !(k < *hi)) return false;
    }
    if (node.leaf) {
      if (!isRoot && static_cast<int>(node.keys.size()) < (m_ + 1) / 2) return false;
      if (node.vals.size() != node.keys.size()) return false;
      if (*leafDepth < 0) *leafDepth = depth;
      if (*leafDepth != depth) return false;
      *count += node.keys.size();
      return true;
    }
    if (node.kids.size() != node.keys.size() + 1) return false;
    if (!isRoot && static_cast<int>(node.keys.size()) < (m_ + 2) / 2 - 1) return false;
    if (isRoot && node.keys.empty()) return false;
    for (size_t i = 0; i < node.kids.size(); ++i) {
      const K* clo = i == 0 ? lo : &node.keys[i - 1];
      const K* chi = i == node.keys.size() ? hi : &node.keys[i];
      if (!checkNode(node.kids[i], depth + 1, clo, chi, leafDepth, count)) return false;
    }
    return true;
  }

  int m_;
  int root_ = 0;
  size_t size_ = 0;
  std::vector<Node> pool_;
};

}
