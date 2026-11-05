#pragma once
#include <string>
#include <vector>

namespace chr {

std::vector<std::string> splitLines(const std::string& text);
std::string joinLines(const std::vector<std::string>& lines);
bool isBinary(const std::string& data);

struct DiffOp {
  char kind;
  int a;
  int b;
};

std::vector<DiffOp> diffSeq(const std::vector<int>& a, const std::vector<int>& b, int maxD = 10000);
std::vector<DiffOp> diffLines(const std::vector<std::string>& a, const std::vector<std::string>& b);

struct Region {
  int bs;
  int be;
  std::vector<std::string> lines;
};
std::vector<Region> regionsFromOps(const std::vector<DiffOp>& ops, const std::vector<std::string>& b);
std::vector<std::string> applyRegions(const std::vector<std::string>& a, const std::vector<Region>& regions);

std::string unifiedHunks(const std::string& a, const std::string& b, int context = 3);

}
