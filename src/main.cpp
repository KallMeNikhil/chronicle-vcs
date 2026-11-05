#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "cli/cli.hpp"

int main(int argc, char** argv) {
  std::vector<std::string> args(argv + 1, argv + argc);
  return chr::cli::run(args);
}
