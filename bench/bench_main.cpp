#include <sys/stat.h>

#include <cstdlib>
#include <map>

#include "bench_util.hpp"

namespace bench {

std::vector<Series>& all() {
  static std::vector<Series> s;
  return s;
}

Series& add(const std::string& id, const std::string& variant, const std::string& theory, const std::string& unit) {
  all().push_back(Series{id, variant, theory, unit, {}});
  return all().back();
}

}

int main(int argc, char** argv) {
  std::string outDir = argc > 1 ? argv[1] : "bench/results";
  std::string only = argc > 2 ? argv[2] : "";
  mkdir(outDir.c_str(), 0755);
  char tmpl[] = "/tmp/chronicle-bench-XXXXXX";
  const char* tmp = mkdtemp(tmpl);
  if (!tmp) return 1;
  bench::all().reserve(64);
  if (only.empty() || only == "structures") bench::benchStructures();
  if (only.empty() || only == "graph") bench::benchGraph();
  if (only.empty() || only == "repo") bench::benchRepo(tmp);
  std::string rm = std::string("rm -rf ") + tmp;
  if (std::system(rm.c_str()) != 0) std::fprintf(stderr, "warning: could not remove %s\n", tmp);

  std::map<std::string, FILE*> files;
  for (const bench::Series& s : bench::all()) {
    FILE*& f = files[s.id];
    if (!f) {
      f = std::fopen((outDir + "/" + s.id + ".csv").c_str(), "w");
      if (!f) return 1;
      std::fprintf(f, "benchmark,variant,n,value,unit\n");
    }
    for (const bench::Point& p : s.pts)
      std::fprintf(f, "%s,\"%s\",%.0f,%.9g,%s\n", s.id.c_str(), s.variant.c_str(), p.n, p.value, s.unit.c_str());
  }
  for (auto& kv : files) std::fclose(kv.second);

  std::printf("\n%-4s %-44s %-10s %s\n", "ID", "variant", "observed", "theoretical exponent");
  std::printf("%s\n", std::string(96, '-').c_str());
  for (const bench::Series& s : bench::all()) {
    if (s.logX)
      std::printf("%-4s %-44s %+8.2f   %s  [slope vs log2 N]\n", s.id.c_str(), s.variant.c_str(),
                  bench::fitLog2Slope(s.pts), s.theory.c_str());
    else
      std::printf("%-4s %-44s %+8.2f   %s\n", s.id.c_str(), s.variant.c_str(), bench::fitExponent(s.pts),
                  s.theory.c_str());
  }
  std::printf("\nCSV files written to %s/\n", outDir.c_str());
  return 0;
}
