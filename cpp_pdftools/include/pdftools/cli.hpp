#pragma once

#include <ostream>
#include <string>
#include <vector>

namespace pdftools {

int RunCli(const std::vector<std::string>& args, std::ostream& out, std::ostream& err);

}  // namespace pdftools

