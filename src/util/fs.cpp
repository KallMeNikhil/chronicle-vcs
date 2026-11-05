#include "util/fs.hpp"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <random>

#include "core/strutil.hpp"

namespace chr {
namespace fsu {

static std::string errText(const std::string& what, const std::string& path) {
  return what + " '" + path + "': " + std::strerror(errno);
}

FileStat lstatPath(const std::string& path) {
  FileStat fs;
  struct stat st;
  if (lstat(path.c_str(), &st) != 0) return fs;
  fs.exists = true;
  fs.isDir = S_ISDIR(st.st_mode);
  fs.isReg = S_ISREG(st.st_mode);
  fs.isLink = S_ISLNK(st.st_mode);
  fs.exec = (st.st_mode & S_IXUSR) != 0;
  fs.size = static_cast<uint64_t>(st.st_size);
  fs.mtimeNs = static_cast<int64_t>(st.st_mtim.tv_sec) * 1000000000LL + st.st_mtim.tv_nsec;
  return fs;
}

bool exists(const std::string& path) { return lstatPath(path).exists; }

bool isDir(const std::string& path) {
  struct stat st;
  return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

Status readFile(const std::string& path, std::string* out) {
  int fd = open(path.c_str(), O_RDONLY);
  if (fd < 0) return Err(errText("cannot read", path));
  out->clear();
  char buf[65536];
  for (;;) {
    ssize_t n = read(fd, buf, sizeof buf);
    if (n < 0) {
      if (errno == EINTR) continue;
      Status s = Err(errText("cannot read", path));
      close(fd);
      return s;
    }
    if (n == 0) break;
    out->append(buf, static_cast<size_t>(n));
  }
  close(fd);
  return Status::Ok();
}

Status readLink(const std::string& path, std::string* out) {
  std::vector<char> buf(4096);
  for (;;) {
    ssize_t n = readlink(path.c_str(), buf.data(), buf.size());
    if (n < 0) return Err(errText("cannot read link", path));
    if (static_cast<size_t>(n) < buf.size()) {
      out->assign(buf.data(), static_cast<size_t>(n));
      return Status::Ok();
    }
    buf.resize(buf.size() * 2);
  }
}

static Status writeAll(int fd, const std::string& data, const std::string& path) {
  size_t off = 0;
  while (off < data.size()) {
    ssize_t n = write(fd, data.data() + off, data.size() - off);
    if (n < 0) {
      if (errno == EINTR) continue;
      return Err(errText("cannot write", path));
    }
    off += static_cast<size_t>(n);
  }
  return Status::Ok();
}

Status writeFile(const std::string& path, const std::string& data, unsigned mode) {
  int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, mode);
  if (fd < 0) return Err(errText("cannot write", path));
  Status s = writeAll(fd, data, path);
  if (close(fd) != 0 && s.ok()) s = Err(errText("cannot close", path));
  return s;
}

std::string uniqueSuffix() {
  static std::atomic<unsigned> counter{0};
  static const unsigned seed = std::random_device{}();
  return std::to_string(getpid()) + "-" + std::to_string(seed) + "-" + std::to_string(counter++);
}

Status writeFileAtomic(const std::string& path, const std::string& data, unsigned mode) {
  std::string tmp = path + ".tmp-" + uniqueSuffix();
  int fd = open(tmp.c_str(), O_WRONLY | O_CREAT | O_EXCL, mode);
  if (fd < 0) return Err(errText("cannot create", tmp));
  Status s = writeAll(fd, data, tmp);
  if (s.ok() && fsync(fd) != 0) s = Err(errText("cannot fsync", tmp));
  if (close(fd) != 0 && s.ok()) s = Err(errText("cannot close", tmp));
  if (s.ok() && rename(tmp.c_str(), path.c_str()) != 0) s = Err(errText("cannot rename into", path));
  if (!s.ok()) unlink(tmp.c_str());
  return s;
}

Status mkdirs(const std::string& path, unsigned mode) {
  if (path.empty() || isDir(path)) return Status::Ok();
  std::string parent = dirName(path);
  if (parent != path) CHR_TRY(mkdirs(parent, mode));
  if (mkdir(path.c_str(), mode) != 0 && errno != EEXIST) return Err(errText("cannot create directory", path));
  return Status::Ok();
}

Status listDir(const std::string& path, std::vector<std::string>* names) {
  names->clear();
  DIR* d = opendir(path.c_str());
  if (!d) return Err(errText("cannot open directory", path));
  while (struct dirent* e = readdir(d)) {
    if (std::strcmp(e->d_name, ".") == 0 || std::strcmp(e->d_name, "..") == 0) continue;
    names->push_back(e->d_name);
  }
  closedir(d);
  std::sort(names->begin(), names->end());
  return Status::Ok();
}

Status removeFile(const std::string& path) {
  if (unlink(path.c_str()) != 0 && errno != ENOENT) return Err(errText("cannot remove", path));
  return Status::Ok();
}

Status removeTree(const std::string& path) {
  FileStat st = lstatPath(path);
  if (!st.exists) return Status::Ok();
  if (!st.isDir) return removeFile(path);
  chmod(path.c_str(), 0755);
  std::vector<std::string> names;
  CHR_TRY(listDir(path, &names));
  for (const std::string& n : names) CHR_TRY(removeTree(joinPath(path, n)));
  if (rmdir(path.c_str()) != 0) return Err(errText("cannot remove directory", path));
  return Status::Ok();
}

void removeEmptyParents(const std::string& path, const std::string& stopAt) {
  std::string d = dirName(path);
  while (d.size() > stopAt.size() && startsWith(d, stopAt)) {
    if (rmdir(d.c_str()) != 0) break;
    d = dirName(d);
  }
}

Status setExecutable(const std::string& path, bool exec) {
  struct stat st;
  if (stat(path.c_str(), &st) != 0) return Err(errText("cannot stat", path));
  mode_t m = st.st_mode & 07777;
  m = exec ? (m | ((m & 0444) >> 2)) : (m & ~static_cast<mode_t>(0111));
  if (chmod(path.c_str(), m) != 0) return Err(errText("cannot chmod", path));
  return Status::Ok();
}

Status makeSymlink(const std::string& target, const std::string& path) {
  if (symlink(target.c_str(), path.c_str()) != 0) return Err(errText("cannot create symlink", path));
  return Status::Ok();
}

std::string joinPath(const std::string& a, const std::string& b) {
  if (a.empty()) return b;
  if (b.empty()) return a;
  if (a.back() == '/') return a + b;
  return a + "/" + b;
}

std::string dirName(const std::string& path) {
  size_t p = path.find_last_of('/');
  if (p == std::string::npos) return ".";
  if (p == 0) return "/";
  return path.substr(0, p);
}

std::string baseName(const std::string& path) {
  size_t p = path.find_last_of('/');
  return p == std::string::npos ? path : path.substr(p + 1);
}

std::string currentDir() {
  std::vector<char> buf(4096);
  while (!getcwd(buf.data(), buf.size())) {
    if (errno != ERANGE) return ".";
    buf.resize(buf.size() * 2);
  }
  return buf.data();
}

std::string absPath(const std::string& path) {
  std::string full = (!path.empty() && path[0] == '/') ? path : joinPath(currentDir(), path);
  std::vector<std::string> parts;
  for (const std::string& c : split(full, '/')) {
    if (c.empty() || c == ".") continue;
    if (c == "..") {
      if (!parts.empty()) parts.pop_back();
      continue;
    }
    parts.push_back(c);
  }
  std::string out;
  for (const std::string& c : parts) out += "/" + c;
  return out.empty() ? "/" : out;
}

}
}
