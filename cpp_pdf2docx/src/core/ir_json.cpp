#include "pdf2docx/ir_json.hpp"

#include <fstream>
#include <iomanip>
#include <string>

namespace pdf2docx {

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

Status WriteIrToJson(const ir::Document& document,
                     const std::string& output_json,
                     const IrJsonOptions& options) {
  if (output_json.empty()) {
    return Status::Error(ErrorCode::kInvalidArgument, "output json path is empty");
  }

  std::ofstream stream(output_json, std::ios::out | std::ios::trunc);
  if (!stream.is_open()) {
    return Status::Error(ErrorCode::kIoError, "cannot open output json");
  }

  size_t total_span_count = 0;
  size_t total_image_count = 0;
  for (const auto& page : document.pages) {
    total_span_count += page.spans.size();
    total_image_count += page.images.size();
  }

  stream << std::fixed << std::setprecision(2);
  stream << "{\n";
  if (options.include_summary) {
    stream << "  \"summary\": {\n";
    stream << "    \"total_pages\": " << document.pages.size() << ",\n";
    stream << "    \"total_spans\": " << total_span_count << ",\n";
    stream << "    \"total_images\": " << total_image_count << "\n";
    stream << "  },\n";
  }

  stream << "  \"pages\": [\n";
  for (size_t page_index = 0; page_index < document.pages.size(); ++page_index) {
    const auto& page = document.pages[page_index];
    stream << "    {\n";
    stream << "      \"index\": " << (page_index + 1) << ",\n";
    stream << "      \"width_pt\": " << page.width_pt << ",\n";
    stream << "      \"height_pt\": " << page.height_pt << ",\n";
    stream << "      \"span_count\": " << page.spans.size() << ",\n";
    stream << "      \"image_count\": " << page.images.size() << ",\n";

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
             << "\"data_size\": " << image.data.size();
      if (options.include_image_quad && image.has_quad) {
        stream << ", \"quad\": {"
               << "\"p0\": {\"x\": " << image.quad.p0.x << ", \"y\": " << image.quad.p0.y << "}, "
               << "\"p1\": {\"x\": " << image.quad.p1.x << ", \"y\": " << image.quad.p1.y << "}, "
               << "\"p2\": {\"x\": " << image.quad.p2.x << ", \"y\": " << image.quad.p2.y << "}, "
               << "\"p3\": {\"x\": " << image.quad.p3.x << ", \"y\": " << image.quad.p3.y << "}"
               << "}";
      }
      stream << "}";
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

  return Status::Ok();
}

}  // namespace pdf2docx
