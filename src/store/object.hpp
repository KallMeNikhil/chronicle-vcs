#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/id.hpp"
#include "core/status.hpp"

namespace chr {

enum class ObjType { None = 0, Blob = 1, Tree = 2, Commit = 3 };

const char* typeName(ObjType t);
bool parseTypeName(const std::string& s, ObjType* t);

std::string objectHeader(ObjType t, size_t payloadSize);
Id objectId(ObjType t, const std::string& payload);
Status decodeObject(const std::string& bytes, ObjType* t, std::string* payload);

constexpr uint32_t kModeFile = 0100644;
constexpr uint32_t kModeExec = 0100755;
constexpr uint32_t kModeDir = 040000;
constexpr uint32_t kModeLink = 0120000;

std::string modeString(uint32_t mode);
std::string modeString6(uint32_t mode);
bool parseMode(const std::string& s, uint32_t* mode);

struct TreeEntry {
  std::string name;
  uint32_t mode = kModeFile;
  Id id;
  bool isDir() const { return mode == kModeDir; }
  bool operator==(const TreeEntry& o) const { return name == o.name && mode == o.mode && id == o.id; }
};

bool validEntryName(const std::string& name);

std::string encodeTree(std::vector<TreeEntry> entries);
Status decodeTree(const std::string& payload, std::vector<TreeEntry>* out);

struct Commit {
  Id tree;
  std::vector<Id> parents;
  std::string author;
  int64_t time = 0;
  int tz = 0;
  std::string message;

  std::string authorName() const;
  std::string summary() const;
};

std::string encodeCommit(const Commit& c);
Status decodeCommit(const std::string& payload, Commit* out);

}
