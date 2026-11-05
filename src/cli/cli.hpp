#pragma once
#include <string>
#include <vector>

#include "core/status.hpp"

namespace chr {
namespace cli {

int run(std::vector<std::string> args);

int report(const Status& s);

class ArgParser {
 public:
  explicit ArgParser(const std::vector<std::string>& args);
  bool flag(const std::string& name);
  bool option(const std::string& name, std::string* value);
  Status finish();
  std::vector<std::string> positional;
  std::vector<std::string> afterDashDash;
  bool sawDashDash = false;

 private:
  std::vector<std::string> toks_;
};

using Handler = int (*)(const std::vector<std::string>& args);

struct Command {
  const char* name;
  Handler fn;
  const char* usage;
};

const std::vector<Command>& commands();

int cmdInit(const std::vector<std::string>& a);
int cmdHashObject(const std::vector<std::string>& a);
int cmdCatFile(const std::vector<std::string>& a);
int cmdAdd(const std::vector<std::string>& a);
int cmdRm(const std::vector<std::string>& a);
int cmdStatus(const std::vector<std::string>& a);
int cmdCommit(const std::vector<std::string>& a);
int cmdBranch(const std::vector<std::string>& a);
int cmdCheckout(const std::vector<std::string>& a);
int cmdReset(const std::vector<std::string>& a);
int cmdFind(const std::vector<std::string>& a);
int cmdDiff(const std::vector<std::string>& a);
int cmdShow(const std::vector<std::string>& a);
int cmdLsTree(const std::vector<std::string>& a);
int cmdLog(const std::vector<std::string>& a);
int cmdGraph(const std::vector<std::string>& a);
int cmdMerge(const std::vector<std::string>& a);
int cmdBlame(const std::vector<std::string>& a);
int cmdReindex(const std::vector<std::string>& a);
int cmdHistory(const std::vector<std::string>& a);
int cmdAt(const std::vector<std::string>& a);
int cmdExport(const std::vector<std::string>& a);
int cmdStats(const std::vector<std::string>& a);
int cmdBisect(const std::vector<std::string>& a);
int cmdPack(const std::vector<std::string>& a);
int cmdGc(const std::vector<std::string>& a);
int cmdVerify(const std::vector<std::string>& a);
int cmdReconcile(const std::vector<std::string>& a);
int cmdCherryPick(const std::vector<std::string>& a);

}
}
