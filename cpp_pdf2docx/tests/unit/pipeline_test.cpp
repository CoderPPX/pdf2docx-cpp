#include <cstdlib>

#include "pipeline/pipeline.hpp"

int main() {
  pdf2docx::pipeline::Pipeline pipeline;
  if (!pipeline.Execute().ok()) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
