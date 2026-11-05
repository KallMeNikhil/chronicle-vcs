#include <cstdio>
#include <cstring>

#include "framework.hpp"

int main(int argc, char** argv) {
  const char* filter = argc > 1 ? argv[1] : nullptr;
  int run = 0;
  for (const tf::TestCase& t : tf::registry()) {
    if (filter && !std::strstr(t.name, filter)) continue;
    int before = tf::failures();
    t.fn();
    ++run;
    std::printf("%s %s\n", tf::failures() == before ? "ok  " : "FAIL", t.name);
  }
  std::printf("%d tests, %d failed checks\n", run, tf::failures());
  return tf::failures() == 0 ? 0 : 1;
}
