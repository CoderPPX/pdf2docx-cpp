#include <cstdlib>
#include <string>

#include "pipeline/pipeline.hpp"

int main() {
  pdf2docx::pipeline::Pipeline pipeline;
  if (pipeline.Execute(nullptr).ok()) {
    return EXIT_FAILURE;
  }

  pdf2docx::ir::Document document;
  pdf2docx::ir::Page page;
  page.spans.push_back(pdf2docx::ir::TextSpan{.text = "bottom-right", .x = 50.0, .y = 100.0});
  page.spans.push_back(pdf2docx::ir::TextSpan{.text = "top-right", .x = 60.0, .y = 700.0});
  page.spans.push_back(pdf2docx::ir::TextSpan{.text = "bottom-left", .x = 10.0, .y = 100.0});
  page.spans.push_back(pdf2docx::ir::TextSpan{.text = "top-left", .x = 5.0, .y = 700.0});
  document.pages.push_back(page);

  pdf2docx::pipeline::PipelineStats stats;
  if (!pipeline.Execute(&document, &stats).ok()) {
    return EXIT_FAILURE;
  }
  if (document.pages.empty() || document.pages[0].spans.size() != 4) {
    return EXIT_FAILURE;
  }
  if (document.pages[0].spans[0].text != "top-left") {
    return EXIT_FAILURE;
  }
  if (document.pages[0].spans[1].text != "top-right") {
    return EXIT_FAILURE;
  }
  if (document.pages[0].spans[2].text != "bottom-left") {
    return EXIT_FAILURE;
  }
  if (document.pages[0].spans[3].text != "bottom-right") {
    return EXIT_FAILURE;
  }
  if (stats.page_count != 1 || stats.reordered_page_count != 1 || stats.reordered_span_count == 0) {
    return EXIT_FAILURE;
  }

  pdf2docx::ir::Document merge_document;
  pdf2docx::ir::Page merge_page;
  merge_page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "Hello", .x = 10.0, .y = 500.0, .length = 20.0});
  merge_page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "world", .x = 35.0, .y = 500.0, .length = 25.0});
  merge_page.spans.push_back(
      pdf2docx::ir::TextSpan{.text = "Second", .x = 10.0, .y = 470.0, .length = 30.0});
  merge_document.pages.push_back(merge_page);

  pdf2docx::pipeline::PipelineStats merge_stats;
  if (!pipeline.Execute(&merge_document, &merge_stats).ok()) {
    return EXIT_FAILURE;
  }
  if (merge_document.pages.size() != 1 || merge_document.pages[0].spans.size() != 2) {
    return EXIT_FAILURE;
  }
  if (merge_document.pages[0].spans[0].text != "Hello world") {
    return EXIT_FAILURE;
  }
  if (merge_document.pages[0].spans[1].text != "Second") {
    return EXIT_FAILURE;
  }
  if (merge_stats.merged_span_count == 0) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
