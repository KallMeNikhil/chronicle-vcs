#include "store/refs.hpp"

#include "core/strutil.hpp"
#include "util/fs.hpp"

namespace chr {

static Status parseIdLine(const std::string& text, const std::string& what, Id* out) {
  if (!Id::fromHex(trim(text), out)) return CorruptErr(what + " does not contain a valid id");
  return Status::Ok();
}

Status Refs::readHead(HeadInfo* out) const {
  *out = HeadInfo();
  std::string text;
  CHR_TRY(fsu::readFile(dir_ + "/HEAD", &text));
  text = trim(text);
  if (startsWith(text, "ref: refs/heads/")) {
    out->branch = text.substr(16);
    if (!branchExists(out->branch)) {
      out->unborn = true;
      return Status::Ok();
    }
    return readBranch(out->branch, &out->id);
  }
  out->detached = true;
  return parseIdLine(text, "HEAD", &out->id);
}

Status Refs::setHeadBranch(const std::string& branch) const {
  return fsu::writeFileAtomic(dir_ + "/HEAD", "ref: refs/heads/" + branch + "\n");
}

Status Refs::setHeadDetached(const Id& id) const {
  return fsu::writeFileAtomic(dir_ + "/HEAD", id.hex() + "\n");
}

Status Refs::advanceHead(const Id& id) const {
  HeadInfo h;
  CHR_TRY(readHead(&h));
  if (h.detached) return setHeadDetached(id);
  return writeBranch(h.branch, id);
}

bool Refs::branchExists(const std::string& name) const {
  return validBranchName(name) && fsu::lstatPath(branchPath(name)).isReg;
}

Status Refs::readBranch(const std::string& name, Id* out) const {
  if (!branchExists(name)) return Err("no such branch '" + name + "'");
  std::string text;
  CHR_TRY(fsu::readFile(branchPath(name), &text));
  return parseIdLine(text, "branch '" + name + "'", out);
}

Status Refs::writeBranch(const std::string& name, const Id& id) const {
  if (!validBranchName(name)) return Err("invalid branch name '" + name + "'");
  std::string p = branchPath(name);
  if (fsu::lstatPath(p).isDir) return Err("branch name '" + name + "' conflicts with an existing branch directory");
  CHR_TRY(fsu::mkdirs(fsu::dirName(p)));
  return fsu::writeFileAtomic(p, id.hex() + "\n");
}

Status Refs::deleteBranch(const std::string& name) const {
  if (!branchExists(name)) return Err("no such branch '" + name + "'");
  CHR_TRY(fsu::removeFile(branchPath(name)));
  fsu::removeEmptyParents(branchPath(name), dir_ + "/refs/heads");
  return Status::Ok();
}

Status Refs::walk(const std::string& rel, std::vector<std::pair<std::string, Id>>* out) const {
  std::string abs = dir_ + "/refs/heads" + (rel.empty() ? "" : "/" + rel);
  std::vector<std::string> names;
  CHR_TRY(fsu::listDir(abs, &names));
  for (const std::string& n : names) {
    std::string r = rel.empty() ? n : rel + "/" + n;
    fsu::FileStat st = fsu::lstatPath(abs + "/" + n);
    if (st.isDir) {
      CHR_TRY(walk(r, out));
    } else if (st.isReg && validBranchName(r) && n.find(".tmp-") == std::string::npos) {
      Id id;
      CHR_TRY(readBranch(r, &id));
      out->push_back({r, id});
    }
  }
  return Status::Ok();
}

Status Refs::listBranches(std::vector<std::pair<std::string, Id>>* out) const {
  out->clear();
  if (!fsu::isDir(dir_ + "/refs/heads")) return Status::Ok();
  return walk("", out);
}

bool Refs::validBranchName(const std::string& n) {
  if (n.empty() || n[0] == '/' || n[0] == '-' || n.back() == '/' || endsWith(n, ".lock")) return false;
  if (n.find("..") != std::string::npos || n.find("//") != std::string::npos) return false;
  if (n == "HEAD" || n == "@") return false;
  for (char c : n) {
    bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' ||
              c == '_' || c == '/' || c == '-';
    if (!ok) return false;
  }
  for (const std::string& comp : split(n, '/'))
    if (comp == "." || comp.empty()) return false;
  return true;
}

}
