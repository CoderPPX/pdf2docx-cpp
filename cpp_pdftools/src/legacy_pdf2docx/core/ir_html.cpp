#include "pdf2docx/ir_html.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace pdf2docx {

namespace {

std::string HtmlEscape(const std::string& input) {
  std::string escaped;
  escaped.reserve(input.size());
  for (char ch : input) {
    switch (ch) {
      case '&':
        escaped += "&amp;";
        break;
      case '<':
        escaped += "&lt;";
        break;
      case '>':
        escaped += "&gt;";
        break;
      case '"':
        escaped += "&quot;";
        break;
      case '\'':
        escaped += "&#39;";
        break;
      default:
        escaped.push_back(ch);
        break;
    }
  }
  return escaped;
}

std::string Base64Encode(const std::vector<uint8_t>& input) {
  static constexpr char kAlphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  std::string output;
  if (input.empty()) {
    return output;
  }

  output.reserve(((input.size() + 2) / 3) * 4);

  size_t index = 0;
  while (index + 3 <= input.size()) {
    const uint32_t value = (static_cast<uint32_t>(input[index]) << 16) |
                           (static_cast<uint32_t>(input[index + 1]) << 8) |
                           static_cast<uint32_t>(input[index + 2]);
    output.push_back(kAlphabet[(value >> 18) & 0x3F]);
    output.push_back(kAlphabet[(value >> 12) & 0x3F]);
    output.push_back(kAlphabet[(value >> 6) & 0x3F]);
    output.push_back(kAlphabet[value & 0x3F]);
    index += 3;
  }

  const size_t remaining = input.size() - index;
  if (remaining == 1) {
    const uint32_t value = static_cast<uint32_t>(input[index]) << 16;
    output.push_back(kAlphabet[(value >> 18) & 0x3F]);
    output.push_back(kAlphabet[(value >> 12) & 0x3F]);
    output.push_back('=');
    output.push_back('=');
  } else if (remaining == 2) {
    const uint32_t value = (static_cast<uint32_t>(input[index]) << 16) |
                           (static_cast<uint32_t>(input[index + 1]) << 8);
    output.push_back(kAlphabet[(value >> 18) & 0x3F]);
    output.push_back(kAlphabet[(value >> 12) & 0x3F]);
    output.push_back(kAlphabet[(value >> 6) & 0x3F]);
    output.push_back('=');
  }

  return output;
}

}  // namespace

Status WriteIrToHtml(const ir::Document& document,
                     const std::string& output_html,
                     const IrHtmlOptions& options) {
  if (output_html.empty()) {
    return Status::Error(ErrorCode::kInvalidArgument, "output html path is empty");
  }
  if (options.scale <= 0.0) {
    return Status::Error(ErrorCode::kInvalidArgument, "html scale must be > 0");
  }
  if (options.only_page > document.pages.size()) {
    return Status::Error(ErrorCode::kInvalidArgument, "requested only_page is out of range");
  }

  std::ofstream stream(output_html, std::ios::out | std::ios::trunc);
  if (!stream.is_open()) {
    return Status::Error(ErrorCode::kIoError, "cannot open output html");
  }

  size_t render_start = 0;
  size_t render_end = document.pages.size();
  if (options.only_page > 0) {
    render_start = options.only_page - 1;
    render_end = render_start + 1;
  }
  const size_t rendered_pages = render_end - render_start;

  stream << std::fixed << std::setprecision(2);
  stream << "<!doctype html>\n";
  stream << "<html><head><meta charset=\"utf-8\"/>\n";
  stream << "<title>pdf2docx ir preview</title>\n";
  stream << "<style>"
            "body{font-family:Arial,sans-serif;padding:16px;background:#f5f5f7;color:#111;}"
            ".meta{margin-bottom:12px;color:#444;}"
            ".page-wrap{margin-bottom:24px;}"
            ".page-title{font-size:14px;margin-bottom:6px;}"
            ".page-canvas{position:relative;background:white;border:1px solid #ddd;box-shadow:0 2px 8px rgba(0,0,0,0.08);overflow:hidden;}"
            ".span{position:absolute;white-space:pre;color:#111;line-height:1.2;}"
            ".span.debug{outline:1px dashed rgba(0,128,255,0.35);}"
            ".image{position:absolute;object-fit:contain;}"
            ".image.debug{outline:1px dashed rgba(255,0,128,0.35);}"
            ".overlay{position:absolute;left:0;top:0;width:100%;height:100%;pointer-events:none;}"
            ".quad{fill:none;stroke:rgba(255,80,0,0.55);stroke-width:1.2;}"
            ".empty{position:absolute;left:10px;top:10px;color:#999;font-style:italic;}"
            "</style>\n";
  stream << "</head><body>\n";
  stream << "<div class=\"meta\">scale=" << options.scale
         << ", pages=" << rendered_pages << "/" << document.pages.size();
  if (options.only_page > 0) {
    stream << ", only_page=" << options.only_page;
  }
  stream << "</div>\n";

  for (size_t page_index = render_start; page_index < render_end; ++page_index) {
    const auto& page = document.pages[page_index];
    const double canvas_width = std::max(1.0, page.width_pt * options.scale);
    const double canvas_height = std::max(1.0, page.height_pt * options.scale);

    stream << "<div class=\"page-wrap\" data-page=\"" << (page_index + 1) << "\">\n";
    stream << "<div class=\"page-title\">Page " << (page_index + 1) << " - "
           << page.width_pt << " x " << page.height_pt << " pt</div>\n";
    stream << "<div class=\"page-canvas\" style=\"width:" << canvas_width
           << "px;height:" << canvas_height << "px\">\n";

    if (page.spans.empty() && page.images.empty()) {
      stream << "<div class=\"empty\">(no extracted text)</div>\n";
    }

    for (const auto& span : page.spans) {
      if (span.text.empty()) {
        continue;
      }

      double left_pt = span.x;
      double bottom_pt = span.y;
      double width_pt = std::max(8.0, span.length);
      double height_pt = 12.0;

      if (span.has_bbox) {
        left_pt = span.bbox.x;
        bottom_pt = span.bbox.y;
        width_pt = std::max(8.0, span.bbox.width);
        height_pt = std::max(8.0, span.bbox.height);
      }

      const double css_left = std::max(0.0, left_pt * options.scale);
      const double css_top = std::max(0.0, (page.height_pt - bottom_pt - height_pt) * options.scale);
      const double css_width = std::max(12.0, width_pt * options.scale);
      const double css_font_size = std::clamp(height_pt * options.scale * 0.8, 9.0, 30.0);

      stream << "<div class=\"span";
      if (options.show_boxes) {
        stream << " debug";
      }
      stream << "\" "
             << "data-x=\"" << span.x << "\" data-y=\"" << span.y << "\" "
             << "style=\"left:" << css_left << "px;top:" << css_top
             << "px;max-width:" << css_width << "px;font-size:" << css_font_size << "px\">"
             << HtmlEscape(span.text) << "</div>\n";
    }

    std::vector<std::string> image_quad_points;
    image_quad_points.reserve(page.images.size());
    for (const auto& image : page.images) {
      if (image.data.empty()) {
        continue;
      }

      const std::string mime_type = image.mime_type.empty() ? "application/octet-stream" : image.mime_type;
      const double width_pt = std::max(1.0, image.width);
      const double height_pt = std::max(1.0, image.height);
      const double css_left = std::max(0.0, image.x * options.scale);
      const double css_top = std::max(0.0, (page.height_pt - image.y - height_pt) * options.scale);
      const double css_width = std::max(1.0, width_pt * options.scale);
      const double css_height = std::max(1.0, height_pt * options.scale);

      stream << "<img class=\"image";
      if (options.show_boxes) {
        stream << " debug";
      }
      stream << "\" "
             << "data-id=\"" << HtmlEscape(image.id) << "\" "
             << "style=\"left:" << css_left << "px;top:" << css_top << "px;"
             << "width:" << css_width << "px;height:" << css_height << "px\" "
             << "src=\"data:" << mime_type << ";base64," << Base64Encode(image.data) << "\"/>\n";

      if (options.show_boxes && image.has_quad) {
        const double q0x = image.quad.p0.x * options.scale;
        const double q0y = (page.height_pt - image.quad.p0.y) * options.scale;
        const double q1x = image.quad.p1.x * options.scale;
        const double q1y = (page.height_pt - image.quad.p1.y) * options.scale;
        const double q2x = image.quad.p2.x * options.scale;
        const double q2y = (page.height_pt - image.quad.p2.y) * options.scale;
        const double q3x = image.quad.p3.x * options.scale;
        const double q3y = (page.height_pt - image.quad.p3.y) * options.scale;

        std::ostringstream points;
        points << q0x << "," << q0y << " "
               << q1x << "," << q1y << " "
               << q3x << "," << q3y << " "
               << q2x << "," << q2y;
        image_quad_points.push_back(points.str());
      }
    }

    if (options.show_boxes && !image_quad_points.empty()) {
      stream << "<svg class=\"overlay\" viewBox=\"0 0 " << canvas_width << " " << canvas_height << "\">\n";
      for (const auto& points : image_quad_points) {
        stream << "<polygon class=\"quad\" points=\"" << points << "\"/>\n";
      }
      stream << "</svg>\n";
    }
    stream << "</div></div>\n";
  }

  stream << "</body></html>\n";
  return Status::Ok();
}

}  // namespace pdf2docx
