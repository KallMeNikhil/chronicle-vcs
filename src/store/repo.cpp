#include "store/repo.hpp"

#include <cstdlib>

#include "util/fs.hpp"

namespace chr {

Status Repo::open(const std::string& start) {
  std::string d = fsu::absPath(start);
  for (;;) {
    if (fsu::isDir(d + "/.chronicle")) {
      work = d;
      dir = d == "/" ? "/.chronicle" : d + "/.chronicle";
      CHR_TRY(config.load(path("config")));
      store.setDir(path("objects"));
      store.setMaxDepth(static_cast<int>(config.getInt("pack.maxdepth", 50)));
      refs.setDir(dir);
      return Status::Ok();
    }
    if (d == "/") break;
    d = fsu::dirName(d);
  }
  return RepoErr("not a chronicle repository (or any parent): " + fsu::absPath(start));
}

Status Repo::init(const std::string& path, std::string* createdDir) {
  std::string root = fsu::absPath(path);
  std::string d = fsu::joinPath(root, ".chronicle");
  if (fsu::exists(d)) return Err("repository already exists in " + d);
  CHR_TRY(fsu::mkdirs(root));
  CHR_TRY(fsu::mkdirs(d + "/objects/pack"));
  CHR_TRY(fsu::mkdirs(d + "/objects/tmp"));
  CHR_TRY(fsu::mkdirs(d + "/refs/heads"));
  CHR_TRY(fsu::writeFileAtomic(d + "/HEAD", "ref: refs/heads/main\n"));
  CHR_TRY(fsu::writeFileAtomic(d + "/config", ""));
  if (createdDir) *createdDir = d;
  return Status::Ok();
}

std::string Repo::authorIdent() const {
  const char* en = std::getenv("CHRONICLE_AUTHOR_NAME");
  const char* ee = std::getenv("CHRONICLE_AUTHOR_EMAIL");
  std::string name = en && *en ? en : config.get("user.name", "Unknown");
  std::string email = ee && *ee ? ee : config.get("user.email", "unknown@localhost");
  return name + " <" + email + ">";
}

}
