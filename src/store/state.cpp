#include "store/state.hpp"

#include "core/strutil.hpp"
#include "util/fs.hpp"

namespace chr {

Status MergeState::load(const std::string& chronicleDir) {
  *this = MergeState();
  std::string path = chronicleDir + "/MERGE_STATE";
  if (!fsu::exists(path)) return Status::Ok();
  std::string text;
  CHR_TRY(fsu::readFile(path, &text));
  active = true;
  for (const std::string& line : split(text, '\n')) {
    if (line.empty()) continue;
    size_t sp = line.find(' ');
    std::string key = line.substr(0, sp), val = sp == std::string::npos ? "" : line.substr(sp + 1);
    std::string un;
    if (key == "kind") kind = val;
    else if (key == "theirs" && Id::fromHex(val, &theirs)) continue;
    else if (key == "branch") branch = val;
    else if (key == "message" && unescapePath(val, &un)) message = un;
    else if (key == "conflict" && unescapePath(val, &un)) conflicts.push_back(un);
    else return CorruptErr("MERGE_STATE: bad line '" + line + "'");
  }
  return Status::Ok();
}

Status MergeState::save(const std::string& chronicleDir) const {
  std::string out = "kind " + kind + "\ntheirs " + theirs.hex() + "\nbranch " + branch + "\n";
  out += "message " + escapePath(message) + "\n";
  for (const std::string& c : conflicts) out += "conflict " + escapePath(c) + "\n";
  return fsu::writeFileAtomic(chronicleDir + "/MERGE_STATE", out);
}

Status MergeState::remove(const std::string& chronicleDir) {
  return fsu::removeFile(chronicleDir + "/MERGE_STATE");
}

bool MergeState::markResolved(const std::string& path) {
  size_t before = conflicts.size();
  std::vector<std::string> keep;
  for (const std::string& c : conflicts)
    if (!(c == path || startsWith(c, path + "/") || path.empty())) keep.push_back(c);
  conflicts.swap(keep);
  return conflicts.size() != before;
}

}
