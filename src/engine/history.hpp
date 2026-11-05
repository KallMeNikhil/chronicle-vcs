#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "engine/dag.hpp"
#include "store/repo.hpp"

namespace chr {

struct HistEvent {
  Id commit;
  char kind;
  uint32_t mode;
  Id blob;
  std::string path;
};

Status commitEvents(ObjectStore& store, const Id& commit, std::vector<HistEvent>* out);
Status appendHistory(Repo& repo, const Id& commit);
Status reindexHistory(Repo& repo, size_t* events);

class HistoryIndex {
 public:
  Status load(Repo& repo);
  Status ensureCovered(Repo& repo, const Dag& dag);

  const std::vector<HistEvent>& events() const { return events_; }
  const std::vector<int>* forPath(const std::string& path) const;
  const std::vector<int>* forBlob(const Id& blob) const;
  std::vector<int> forPrefix(const std::string& dir) const;

 private:
  void addEvent(const HistEvent& e);
  std::vector<HistEvent> events_;
  std::unordered_map<std::string, std::vector<int>> byPath_;
  std::unordered_map<Id, std::vector<int>, IdHash> byBlob_;
  std::unordered_set<Id, IdHash> commits_;
};

}
