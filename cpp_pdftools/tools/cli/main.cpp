#include <iostream>
#include <string>
#include <vector>

#include "pdftools/cli.hpp"

int main(int argc, char** argv) {
  std::vector<std::string> args;
  args.reserve(static_cast<size_t>(argc));
  for (int i = 0; i < argc; ++i) {
    args.emplace_back(argv[i]);
  }
  return pdftools::RunCli(args, std::cout, std::cerr);
}

