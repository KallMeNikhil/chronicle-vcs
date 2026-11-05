#pragma once
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace bench {

struct Point {
  double n;
  double value;
};

struct Series {
  std::string id;
  std::string variant;
  std::string theory;
  std::string unit = "s";
  std::vector<Point> pts;
  bool logX = false;
};

std::vector<Series>& all();

inline double timeIt(const std::function<void()>& fn, int reps = 3) {
  double best = 1e100;
  for (int r = 0; r < reps; ++r) {
    auto t0 = std::chrono::steady_clock::now();
    fn();
    auto t1 = std::chrono::steady_clock::now();
    best = std::min(best, std::chrono::duration<double>(t1 - t0).count());
  }
  return best;
}

inline double fitLog2Slope(const std::vector<Point>& pts) {
  double sx = 0, sy = 0, sxx = 0, sxy = 0;
  int k = 0;
  for (const Point& p : pts) {
    double x = std::log2(p.n), y = p.value;
    sx += x, sy += y, sxx += x * x, sxy += x * y;
    ++k;
  }
  if (k < 2) return 0;
  return (k * sxy - sx * sy) / (k * sxx - sx * sx);
}

inline double fitExponent(const std::vector<Point>& pts) {
  double sx = 0, sy = 0, sxx = 0, sxy = 0;
  int k = 0;
  for (const Point& p : pts) {
    if (p.n <= 0 || p.value <= 0) continue;
    double x = std::log(p.n), y = std::log(p.value);
    sx += x, sy += y, sxx += x * x, sxy += x * y;
    ++k;
  }
  if (k < 2) return 0;
  return (k * sxy - sx * sy) / (k * sxx - sx * sx);
}

template <typename T>
inline void keep(const T& v) {
  __asm__ __volatile__("" : : "g"(&v) : "memory");
}

Series& add(const std::string& id, const std::string& variant, const std::string& theory,
            const std::string& unit = "s");

void benchStructures();
void benchGraph();
void benchRepo(const std::string& tmpRoot);

}
