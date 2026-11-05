#include <unistd.h>

#include <cstdio>

#include "cli/cli.hpp"
#include "core/strutil.hpp"

namespace chr {
namespace cli {

static const char* kVersion = "chronicle 0.1.0";

const std::vector<Command>& commands() {
  static const std::vector<Command> table = {
      {"init", cmdInit, "init [dir]"},
      {"hash-object", cmdHashObject, "hash-object [-w] <file>"},
      {"cat-file", cmdCatFile, "cat-file -t|-s|-p <id>"},
      {"add", cmdAdd, "add <path>..."},
      {"rm", cmdRm, "rm [--cached] <path>..."},
      {"status", cmdStatus, "status"},
      {"commit", cmdCommit, "commit -m <msg>"},
      {"branch", cmdBranch, "branch [<name> [<rev>]] [-d <name>] [--list]"},
      {"checkout", cmdCheckout, "checkout <branch|rev> [-b <newbranch>]"},
      {"reset", cmdReset, "reset --hard [<rev>]"},
      {"find", cmdFind, "find <prefix>"},
      {"diff", cmdDiff, "diff [--cached] [<rev> [<rev>]] [-- <path>]"},
      {"show", cmdShow, "show <rev-or-id>"},
      {"ls-tree", cmdLsTree, "ls-tree [-r] <rev> [<path>]"},
      {"log", cmdLog, "log [--oneline] [--graph] [--since T] [--until T] [-n N] [<rev>] [-- <path>]"},
      {"graph", cmdGraph, "graph [--dot] [<rev>...]"},
      {"merge", cmdMerge, "merge <branch> [--no-ff] [--abort] [--allow-unrelated]"},
      {"blame", cmdBlame, "blame <path> [<rev>]"},
      {"reindex", cmdReindex, "reindex"},
      {"history", cmdHistory, "history <path> | history --first <path> | history --blob <id>"},
      {"at", cmdAt, "at <rev-or-time> <path>"},
      {"export", cmdExport, "export <rev-or-time> <dir>"},
      {"stats", cmdStats, "stats [--sharing] [--last N]"},
      {"pack", cmdPack, "pack [--keep-loose]"},
      {"gc", cmdGc, "gc [--dry-run] [--grace-seconds N]"},
      {"verify", cmdVerify, "verify"},
      {"reconcile", cmdReconcile, "reconcile <other-repo-dir> [--fetch]   (EXPERIMENTAL)"},
      {"cherry-pick", cmdCherryPick, "cherry-pick <rev>   (EXPERIMENTAL)"},
      {"bisect", cmdBisect, "bisect start [--strategy=linear|weighted] | bad [<rev>] | good [<rev>] | skip [<rev>] | run <cmd...> | reset | log"},
  };
  return table;
}

int report(const Status& s) {
  if (s.ok()) return 0;
  if (!s.msg.empty()) std::fprintf(stderr, "chronicle: %s\n", s.msg.c_str());
  return s.code;
}

static void printHelp() {
  std::printf("usage: chronicle [-C <dir>] <command> [<args>]\n\ncommands:\n");
  for (const Command& c : commands()) std::printf("  %s\n", c.usage);
}

int run(std::vector<std::string> args) {
  size_t i = 0;
  while (i < args.size()) {
    if (args[i] == "-C") {
      if (i + 1 >= args.size()) return report(UsageErr("-C needs a directory"));
      if (chdir(args[i + 1].c_str()) != 0)
        return report(Err("cannot change to directory '" + args[i + 1] + "'"));
      i += 2;
    } else if (args[i] == "--version") {
      std::printf("%s\n", kVersion);
      return 0;
    } else if (args[i] == "--help" || args[i] == "-h") {
      printHelp();
      return 0;
    } else {
      break;
    }
  }
  if (i >= args.size()) {
    printHelp();
    return kErrUsage;
  }
  const std::string name = args[i];
  std::vector<std::string> rest(args.begin() + static_cast<long>(i) + 1, args.end());
  for (const Command& c : commands()) {
    if (name == c.name) {
      if (!rest.empty() && (rest[0] == "--help" || rest[0] == "-h")) {
        std::printf("usage: chronicle %s\n", c.usage);
        return 0;
      }
      return c.fn(rest);
    }
  }
  return report(UsageErr("unknown command '" + name + "' (see chronicle --help)"));
}

ArgParser::ArgParser(const std::vector<std::string>& args) {
  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "--") {
      sawDashDash = true;
      afterDashDash.assign(args.begin() + static_cast<long>(i) + 1, args.end());
      break;
    }
    toks_.push_back(args[i]);
  }
}

bool ArgParser::flag(const std::string& name) {
  for (size_t i = 0; i < toks_.size(); ++i) {
    if (toks_[i] == name) {
      toks_.erase(toks_.begin() + static_cast<long>(i));
      return true;
    }
  }
  return false;
}

bool ArgParser::option(const std::string& name, std::string* value) {
  for (size_t i = 0; i < toks_.size(); ++i) {
    if (toks_[i] == name && i + 1 < toks_.size()) {
      *value = toks_[i + 1];
      toks_.erase(toks_.begin() + static_cast<long>(i), toks_.begin() + static_cast<long>(i) + 2);
      return true;
    }
    if (startsWith(toks_[i], name + "=")) {
      *value = toks_[i].substr(name.size() + 1);
      toks_.erase(toks_.begin() + static_cast<long>(i));
      return true;
    }
  }
  return false;
}

Status ArgParser::finish() {
  positional.clear();
  for (const std::string& t : toks_) {
    if (t.size() > 1 && t[0] == '-') return UsageErr("unknown option '" + t + "'");
    positional.push_back(t);
  }
  return Status::Ok();
}

}
}
