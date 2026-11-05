#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "engine/dag.hpp"
#include "engine/history.hpp"

namespace chr {

class Blamer {
 public:
  Blamer(ObjectStore& store, const Dag& dag, const HistoryIndex& hist) : store_(store), dag_(dag), hist_(hist) {}
  void setMemo(bool on) { useMemo_ = on; }

  Status blame(const Id& rev, const std::string& path, std::vector<int>* origins,
               std::vector<std::string>* lines);
  size_t diffsRun() const { return diffs_; }

 private:
  struct State {
    bool present = false;
    std::shared_ptr<const std::vector<int>> origin;
    std::shared_ptr<const std::vector<std::string>> lines;
  };
  using Key = uint64_t;

  int rep(int x, const std::unordered_map<int, int>& touched);
  Status compute(int c, int pathId, const HistEvent& ev, const std::unordered_map<int, int>& touched);

  ObjectStore& store_;
  const Dag& dag_;
  const HistoryIndex& hist_;
  bool useMemo_ = true;
  size_t diffs_ = 0;
  std::unordered_map<std::string, int> pathIds_;
  std::unordered_map<Key, State> memo_;
};

}
