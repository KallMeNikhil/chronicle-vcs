#include "store/object.hpp"

#include <algorithm>
#include <cstdio>

#include "core/sha256.hpp"
#include "core/strutil.hpp"
#include "core/timeutil.hpp"

namespace chr {

const char* typeName(ObjType t) {
  switch (t) {
    case ObjType::Blob: return "blob";
    case ObjType::Tree: return "tree";
    case ObjType::Commit: return "commit";
    default: return "none";
  }
}

bool parseTypeName(const std::string& s, ObjType* t) {
  if (s == "blob") *t = ObjType::Blob;
  else if (s == "tree") *t = ObjType::Tree;
  else if (s == "commit") *t = ObjType::Commit;
  else return false;
  return true;
}

std::string objectHeader(ObjType t, size_t payloadSize) {
  std::string h = typeName(t);
  h += ' ';
  h += std::to_string(payloadSize);
  h += '\0';
  return h;
}

Id objectId(ObjType t, const std::string& payload) {
  Sha256 h;
  h.update(objectHeader(t, payload.size()));
  h.update(payload);
  return h.finish();
}

Status decodeObject(const std::string& bytes, ObjType* t, std::string* payload) {
  size_t nul = bytes.find('\0');
  if (nul == std::string::npos || nul > 32) return CorruptErr("object header missing");
  std::string header = bytes.substr(0, nul);
  size_t sp = header.find(' ');
  if (sp == std::string::npos) return CorruptErr("bad object header");
  if (!parseTypeName(header.substr(0, sp), t)) return CorruptErr("unknown object type '" + header.substr(0, sp) + "'");
  int64_t size = 0;
  if (!parseInt64(header.substr(sp + 1), &size) || size < 0 ||
      static_cast<size_t>(size) != bytes.size() - nul - 1)
    return CorruptErr("object size mismatch");
  payload->assign(bytes, nul + 1, std::string::npos);
  return Status::Ok();
}

std::string modeString(uint32_t mode) {
  char buf[16];
  std::snprintf(buf, sizeof buf, "%o", mode);
  return buf;
}

std::string modeString6(uint32_t mode) {
  char buf[16];
  std::snprintf(buf, sizeof buf, "%06o", mode);
  return buf;
}

bool parseMode(const std::string& s, uint32_t* mode) {
  if (s == "100644") *mode = kModeFile;
  else if (s == "100755") *mode = kModeExec;
  else if (s == "40000") *mode = kModeDir;
  else if (s == "120000") *mode = kModeLink;
  else return false;
  return true;
}

bool validEntryName(const std::string& name) {
  if (name.empty() || name == "." || name == ".." || name == ".chronicle") return false;
  return name.find('/') == std::string::npos && name.find('\0') == std::string::npos;
}

std::string encodeTree(std::vector<TreeEntry> entries) {
  std::sort(entries.begin(), entries.end(),
            [](const TreeEntry& a, const TreeEntry& b) { return a.name < b.name; });
  std::string out;
  for (const TreeEntry& e : entries) {
    out += modeString(e.mode);
    out += ' ';
    out += e.name;
    out += '\0';
    out.append(reinterpret_cast<const char*>(e.id.b), 32);
  }
  return out;
}

Status decodeTree(const std::string& payload, std::vector<TreeEntry>* out) {
  out->clear();
  size_t pos = 0;
  while (pos < payload.size()) {
    size_t sp = payload.find(' ', pos);
    if (sp == std::string::npos) return CorruptErr("bad tree entry");
    size_t nul = payload.find('\0', sp + 1);
    if (nul == std::string::npos || nul + 33 > payload.size()) return CorruptErr("truncated tree entry");
    TreeEntry e;
    if (!parseMode(payload.substr(pos, sp - pos), &e.mode)) return CorruptErr("bad tree entry mode");
    e.name = payload.substr(sp + 1, nul - sp - 1);
    if (!validEntryName(e.name)) return CorruptErr("bad tree entry name '" + escapePath(e.name) + "'");
    e.id = Id::fromRaw(payload.data() + nul + 1);
    if (!out->empty() && !(out->back().name < e.name)) return CorruptErr("tree entries out of order");
    out->push_back(e);
    pos = nul + 33;
  }
  return Status::Ok();
}

std::string Commit::authorName() const {
  size_t lt = author.find(" <");
  return lt == std::string::npos ? author : author.substr(0, lt);
}

std::string Commit::summary() const {
  size_t nl = message.find('\n');
  return nl == std::string::npos ? message : message.substr(0, nl);
}

std::string encodeCommit(const Commit& c) {
  std::string out = "tree " + c.tree.hex() + "\n";
  for (const Id& p : c.parents) out += "parent " + p.hex() + "\n";
  out += "author " + c.author + " " + std::to_string(c.time) + " " + formatTz(c.tz) + "\n\n";
  out += c.message;
  if (out.empty() || out.back() != '\n') out += '\n';
  return out;
}

Status decodeCommit(const std::string& payload, Commit* out) {
  *out = Commit();
  size_t pos = 0;
  bool haveTree = false, haveAuthor = false;
  for (;;) {
    size_t nl = payload.find('\n', pos);
    if (nl == std::string::npos) return CorruptErr("commit header not terminated");
    std::string line = payload.substr(pos, nl - pos);
    pos = nl + 1;
    if (line.empty()) break;
    if (startsWith(line, "tree ")) {
      if (!Id::fromHex(line.substr(5), &out->tree)) return CorruptErr("bad commit tree id");
      haveTree = true;
    } else if (startsWith(line, "parent ")) {
      Id p;
      if (!Id::fromHex(line.substr(7), &p)) return CorruptErr("bad commit parent id");
      out->parents.push_back(p);
    } else if (startsWith(line, "author ")) {
      std::string rest = line.substr(7);
      size_t s2 = rest.rfind(' ');
      if (s2 == std::string::npos) return CorruptErr("bad author line");
      size_t s1 = rest.rfind(' ', s2 - 1);
      if (s1 == std::string::npos || s2 == 0) return CorruptErr("bad author line");
      if (!parseInt64(rest.substr(s1 + 1, s2 - s1 - 1), &out->time) || !parseTz(rest.substr(s2 + 1), &out->tz))
        return CorruptErr("bad author timestamp");
      out->author = rest.substr(0, s1);
      haveAuthor = true;
    } else {
      return CorruptErr("unknown commit header '" + line + "'");
    }
  }
  if (!haveTree || !haveAuthor) return CorruptErr("commit missing tree or author");
  out->message = payload.substr(pos);
  return Status::Ok();
}

}
