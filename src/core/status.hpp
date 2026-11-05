#pragma once
#include <string>
#include <utility>

namespace chr {

enum ExitCode : int {
  kOk = 0,
  kErrGeneral = 1,
  kErrUsage = 2,
  kErrConflict = 3,
  kErrCorrupt = 4,
  kErrRepo = 5,
};

struct Status {
  int code = kOk;
  std::string msg;
  bool ok() const { return code == kOk; }
  static Status Ok() { return Status(); }
};

inline Status Err(std::string m) { return Status{kErrGeneral, std::move(m)}; }
inline Status UsageErr(std::string m) { return Status{kErrUsage, std::move(m)}; }
inline Status CorruptErr(std::string m) { return Status{kErrCorrupt, std::move(m)}; }
inline Status RepoErr(std::string m) { return Status{kErrRepo, std::move(m)}; }
inline Status ConflictErr(std::string m) { return Status{kErrConflict, std::move(m)}; }

}

#define CHR_TRY(expr)                    \
  do {                                   \
    ::chr::Status chr_s_ = (expr);       \
    if (!chr_s_.ok()) return chr_s_;     \
  } while (0)
