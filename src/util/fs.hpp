#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/status.hpp"

namespace chr {
namespace fsu {

struct FileStat {
  bool exists = false;
  bool isDir = false;
  bool isReg = false;
  bool isLink = false;
  bool exec = false;
  uint64_t size = 0;
  int64_t mtimeNs = 0;
};

FileStat lstatPath(const std::string& path);
bool exists(const std::string& path);
bool isDir(const std::string& path);

Status readFile(const std::string& path, std::string* out);
Status readLink(const std::string& path, std::string* out);
Status writeFile(const std::string& path, const std::string& data, unsigned mode = 0644);
Status writeFileAtomic(const std::string& path, const std::string& data, unsigned mode = 0644);
Status mkdirs(const std::string& path, unsigned mode = 0755);
Status listDir(const std::string& path, std::vector<std::string>* names);
Status removeFile(const std::string& path);
Status removeTree(const std::string& path);
void removeEmptyParents(const std::string& path, const std::string& stopAt);
Status setExecutable(const std::string& path, bool exec);
Status makeSymlink(const std::string& target, const std::string& path);

std::string joinPath(const std::string& a, const std::string& b);
std::string dirName(const std::string& path);
std::string baseName(const std::string& path);
std::string currentDir();
std::string absPath(const std::string& path);
std::string uniqueSuffix();

}
}
