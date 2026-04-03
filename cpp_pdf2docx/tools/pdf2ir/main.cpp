#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

#include "pdf2docx/converter.hpp"

namespace {

std::string JsonEscape(const std::string& input) {
  std::string escaped;
  escaped.reserve(input.size());
  for (char ch : input) {
    switch (ch) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        escaped.push_back(ch);
        break;
    }
  }
  return escaped;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "Usage: pdf2ir <input.pdf> <output.json>\n";
    return 1;
  }

  const std::string input_pdf = argv[1];
  const std::string output_json = argv[2];

  pdf2docx::Converter converter(pdf2docx::BackendKind::kPoDoFo);
  pdf2docx::ConvertOptions options;
  pdf2docx::ir::Document document;
  const pdf2docx::Status status = converter.ExtractIrFromFile(input_pdf, options, &document);
  if (!status.ok()) {
    std::cerr << "Extract IR failed: " << status.message() << "\n";
    return 2;
  }

  std::ofstream stream(output_json, std::ios::out | std::ios::trunc);
  if (!stream.is_open()) {
    std::cerr << "Cannot open output json: " << output_json << "\n";
    return 3;
  }

  stream << std::fixed << std::setprecision(2);
  stream << "{\n";
  stream << "  \"pages\": [\n";
  for (size_t page_index = 0; page_index < document.pages.size(); ++page_index) {
    const auto& page = document.pages[page_index];
    stream << "    {\n";
    stream << "      \"index\": " << (page_index + 1) << ",\n";
    stream << "      \"width_pt\": " << page.width_pt << ",\n";
    stream << "      \"height_pt\": " << page.height_pt << ",\n";

    stream << "      \"spans\": [\n";
    for (size_t span_index = 0; span_index < page.spans.size(); ++span_index) {
      const auto& span = page.spans[span_index];
      stream << "        {\"text\": \"" << JsonEscape(span.text) << "\", "
             << "\"x\": " << span.x << ", "
             << "\"y\": " << span.y << ", "
             << "\"length\": " << span.length;
      if (span.has_bbox) {
        stream << ", \"bbox\": {\"x\": " << span.bbox.x << ", \"y\": " << span.bbox.y
               << ", \"width\": " << span.bbox.width << ", \"height\": " << span.bbox.height << "}";
      }
      stream << "}";
      if (span_index + 1 < page.spans.size()) {
        stream << ",";
      }
      stream << "\n";
    }
    stream << "      ],\n";

    stream << "      \"images\": [\n";
    for (size_t image_index = 0; image_index < page.images.size(); ++image_index) {
      const auto& image = page.images[image_index];
      stream << "        {\"id\": \"" << JsonEscape(image.id) << "\", "
             << "\"mime_type\": \"" << JsonEscape(image.mime_type) << "\", "
             << "\"extension\": \"" << JsonEscape(image.extension) << "\", "
             << "\"x\": " << image.x << ", "
             << "\"y\": " << image.y << ", "
             << "\"width\": " << image.width << ", "
             << "\"height\": " << image.height << ", "
             << "\"data_size\": " << image.data.size() << "}";
      if (image_index + 1 < page.images.size()) {
        stream << ",";
      }
      stream << "\n";
    }
    stream << "      ]\n";
    stream << "    }";
    if (page_index + 1 < document.pages.size()) {
      stream << ",";
    }
    stream << "\n";
  }
  stream << "  ]\n";
  stream << "}\n";

  std::cout << "IR written: pages=" << document.pages.size() << " -> " << output_json << "\n";
  return 0;
}
