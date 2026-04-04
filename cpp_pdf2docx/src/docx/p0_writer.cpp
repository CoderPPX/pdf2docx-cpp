#include "docx/p0_writer.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#if PDF2DOCX_HAS_TINYXML2
#include <tinyxml2.h>
#endif

#if PDF2DOCX_HAS_MINIZIP
#include <zlib.h>
#include <zip.h>
#endif

namespace pdf2docx::docx {

namespace {

struct DocxImageRef {
  size_t page_index = 0;
  std::string relationship_id;
  std::string part_name;
  std::string file_name;
  std::string extension;
  std::string mime_type;
  std::vector<uint8_t> data;
  int64_t width_emu = 1;
  int64_t height_emu = 1;
  int64_t x_emu = 0;
  int64_t y_emu = 0;
  int32_t rotation_60000 = 0;
};

struct ImageBounds {
  double left = 0.0;
  double bottom = 0.0;
  double width = 1.0;
  double height = 1.0;
};

int64_t PtToEmu(double pt) {
  constexpr double kEmuPerPt = 12700.0;
  return std::max<int64_t>(1, static_cast<int64_t>(std::llround(pt * kEmuPerPt)));
}

double SpanLeftX(const ir::TextSpan& span) {
  if (span.has_bbox) {
    return span.bbox.x;
  }
  return span.x;
}

double SpanRightX(const ir::TextSpan& span) {
  if (span.has_bbox) {
    return span.bbox.x + span.bbox.width;
  }
  return span.x + std::max(8.0, span.length);
}

double SpanLineY(const ir::TextSpan& span) {
  if (span.has_bbox && span.bbox.height > 0.0) {
    return span.bbox.y + span.bbox.height;
  }
  return span.y;
}

std::string NormalizeExtension(const std::string& extension) {
  std::string normalized = extension;
  if (!normalized.empty() && normalized.front() == '.') {
    normalized.erase(normalized.begin());
  }
  std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return normalized;
}

std::string GuessMimeTypeFromExtension(const std::string& extension) {
  if (extension == "jpg" || extension == "jpeg") {
    return "image/jpeg";
  }
  if (extension == "png") {
    return "image/png";
  }
  if (extension == "gif") {
    return "image/gif";
  }
  if (extension == "bmp") {
    return "image/bmp";
  }
  if (extension == "jp2") {
    return "image/jp2";
  }
  return "application/octet-stream";
}

ImageBounds ResolveImageBounds(const ir::ImageBlock& image) {
  if (!image.has_quad) {
    return ImageBounds{
        .left = image.x,
        .bottom = image.y,
        .width = std::max(1.0, image.width),
        .height = std::max(1.0, image.height),
    };
  }

  const std::array<ir::Point, 4> points = {
      image.quad.p0,
      image.quad.p1,
      image.quad.p2,
      image.quad.p3,
  };
  double min_x = points[0].x;
  double max_x = points[0].x;
  double min_y = points[0].y;
  double max_y = points[0].y;
  for (const auto& point : points) {
    min_x = std::min(min_x, point.x);
    max_x = std::max(max_x, point.x);
    min_y = std::min(min_y, point.y);
    max_y = std::max(max_y, point.y);
  }
  return ImageBounds{
      .left = min_x,
      .bottom = min_y,
      .width = std::max(1.0, max_x - min_x),
      .height = std::max(1.0, max_y - min_y),
  };
}

int32_t ResolveImageRotation60000(const ir::ImageBlock& image) {
  if (!image.has_quad) {
    return 0;
  }
  const double dx = image.quad.p1.x - image.quad.p0.x;
  const double dy = image.quad.p1.y - image.quad.p0.y;
  if (std::fabs(dx) < 1e-6 && std::fabs(dy) < 1e-6) {
    return 0;
  }
  constexpr double kRadToDeg = 57.29577951308232;
  const double degrees = std::atan2(dy, dx) * kRadToDeg;
  if (std::fabs(degrees) < 0.1) {
    return 0;
  }
  return static_cast<int32_t>(std::llround(degrees * 60000.0));
}

std::vector<DocxImageRef> CollectDocxImages(const ir::Document& ir_document) {
  std::vector<DocxImageRef> refs;
  uint32_t index = 1;

  for (size_t page_index = 0; page_index < ir_document.pages.size(); ++page_index) {
    const auto& page = ir_document.pages[page_index];
    for (const auto& image : page.images) {
      if (image.data.empty()) {
        continue;
      }

      std::string extension = NormalizeExtension(image.extension);
      if (extension.empty()) {
        continue;
      }

      const std::string mime_type =
          image.mime_type.empty() ? GuessMimeTypeFromExtension(extension) : image.mime_type;

      DocxImageRef ref;
      ref.page_index = page_index;
      ref.relationship_id = "rId" + std::to_string(index);
      ref.extension = extension;
      ref.mime_type = mime_type;
      ref.file_name = "image" + std::to_string(index) + "." + extension;
      ref.part_name = "word/media/" + ref.file_name;
      ref.data = image.data;
      const ImageBounds bounds = ResolveImageBounds(image);
      ref.width_emu = PtToEmu(bounds.width);
      ref.height_emu = PtToEmu(bounds.height);
      ref.x_emu = PtToEmu(std::max(0.0, bounds.left));
      const double top_pt = std::max(0.0, page.height_pt - (bounds.bottom + bounds.height));
      ref.y_emu = PtToEmu(top_pt);
      ref.rotation_60000 = ResolveImageRotation60000(image);
      refs.push_back(std::move(ref));
      ++index;
    }
  }

  return refs;
}

#if PDF2DOCX_HAS_TINYXML2
void AppendImageGraphic(tinyxml2::XMLDocument& document,
                        tinyxml2::XMLElement* parent,
                        const DocxImageRef& image,
                        uint32_t drawing_id) {
  auto* graphic = document.NewElement("a:graphic");
  parent->InsertEndChild(graphic);

  auto* graphic_data = document.NewElement("a:graphicData");
  graphic_data->SetAttribute("uri", "http://schemas.openxmlformats.org/drawingml/2006/picture");
  graphic->InsertEndChild(graphic_data);

  auto* pic = document.NewElement("pic:pic");
  graphic_data->InsertEndChild(pic);

  auto* nv_pic_pr = document.NewElement("pic:nvPicPr");
  pic->InsertEndChild(nv_pic_pr);

  auto* c_nv_pr = document.NewElement("pic:cNvPr");
  c_nv_pr->SetAttribute("id", "0");
  c_nv_pr->SetAttribute("name", image.file_name.c_str());
  nv_pic_pr->InsertEndChild(c_nv_pr);

  auto* c_nv_pic_pr = document.NewElement("pic:cNvPicPr");
  nv_pic_pr->InsertEndChild(c_nv_pic_pr);

  auto* blip_fill = document.NewElement("pic:blipFill");
  pic->InsertEndChild(blip_fill);

  auto* blip = document.NewElement("a:blip");
  blip->SetAttribute("r:embed", image.relationship_id.c_str());
  blip_fill->InsertEndChild(blip);

  auto* stretch = document.NewElement("a:stretch");
  blip_fill->InsertEndChild(stretch);

  auto* fill_rect = document.NewElement("a:fillRect");
  stretch->InsertEndChild(fill_rect);

  auto* sp_pr = document.NewElement("pic:spPr");
  pic->InsertEndChild(sp_pr);

  auto* xfrm = document.NewElement("a:xfrm");
  if (image.rotation_60000 != 0) {
    const std::string rotation = std::to_string(image.rotation_60000);
    xfrm->SetAttribute("rot", rotation.c_str());
  }
  sp_pr->InsertEndChild(xfrm);

  auto* off = document.NewElement("a:off");
  off->SetAttribute("x", "0");
  off->SetAttribute("y", "0");
  xfrm->InsertEndChild(off);

  const std::string width_emu = std::to_string(image.width_emu);
  const std::string height_emu = std::to_string(image.height_emu);
  auto* ext = document.NewElement("a:ext");
  ext->SetAttribute("cx", width_emu.c_str());
  ext->SetAttribute("cy", height_emu.c_str());
  xfrm->InsertEndChild(ext);

  auto* prst_geom = document.NewElement("a:prstGeom");
  prst_geom->SetAttribute("prst", "rect");
  sp_pr->InsertEndChild(prst_geom);

  auto* av_lst = document.NewElement("a:avLst");
  prst_geom->InsertEndChild(av_lst);

  (void)drawing_id;
}

void AppendImageDrawing(tinyxml2::XMLDocument& document,
                        tinyxml2::XMLElement* run,
                        const DocxImageRef& image,
                        bool use_anchored_images,
                        uint32_t* drawing_id) {
  auto* drawing = document.NewElement("w:drawing");
  run->InsertEndChild(drawing);

  const std::string width_emu = std::to_string(image.width_emu);
  const std::string height_emu = std::to_string(image.height_emu);
  const std::string x_emu = std::to_string(image.x_emu);
  const std::string y_emu = std::to_string(image.y_emu);
  const std::string drawing_id_str = std::to_string(*drawing_id);
  ++(*drawing_id);

  if (use_anchored_images) {
    auto* anchor = document.NewElement("wp:anchor");
    anchor->SetAttribute("distT", "0");
    anchor->SetAttribute("distB", "0");
    anchor->SetAttribute("distL", "0");
    anchor->SetAttribute("distR", "0");
    anchor->SetAttribute("simplePos", "0");
    anchor->SetAttribute("relativeHeight", "251659264");
    anchor->SetAttribute("behindDoc", "0");
    anchor->SetAttribute("locked", "0");
    anchor->SetAttribute("layoutInCell", "1");
    anchor->SetAttribute("allowOverlap", "1");
    drawing->InsertEndChild(anchor);

    auto* simple_pos = document.NewElement("wp:simplePos");
    simple_pos->SetAttribute("x", "0");
    simple_pos->SetAttribute("y", "0");
    anchor->InsertEndChild(simple_pos);

    auto* position_h = document.NewElement("wp:positionH");
    position_h->SetAttribute("relativeFrom", "page");
    auto* pos_h_offset = document.NewElement("wp:posOffset");
    pos_h_offset->SetText(x_emu.c_str());
    position_h->InsertEndChild(pos_h_offset);
    anchor->InsertEndChild(position_h);

    auto* position_v = document.NewElement("wp:positionV");
    position_v->SetAttribute("relativeFrom", "page");
    auto* pos_v_offset = document.NewElement("wp:posOffset");
    pos_v_offset->SetText(y_emu.c_str());
    position_v->InsertEndChild(pos_v_offset);
    anchor->InsertEndChild(position_v);

    auto* extent = document.NewElement("wp:extent");
    extent->SetAttribute("cx", width_emu.c_str());
    extent->SetAttribute("cy", height_emu.c_str());
    anchor->InsertEndChild(extent);

    auto* wrap_none = document.NewElement("wp:wrapNone");
    anchor->InsertEndChild(wrap_none);

    auto* doc_pr = document.NewElement("wp:docPr");
    doc_pr->SetAttribute("id", drawing_id_str.c_str());
    doc_pr->SetAttribute("name", image.file_name.c_str());
    anchor->InsertEndChild(doc_pr);

    AppendImageGraphic(document, anchor, image, *drawing_id);
    return;
  }

  auto* inline_drawing = document.NewElement("wp:inline");
  inline_drawing->SetAttribute("distT", "0");
  inline_drawing->SetAttribute("distB", "0");
  inline_drawing->SetAttribute("distL", "0");
  inline_drawing->SetAttribute("distR", "0");
  drawing->InsertEndChild(inline_drawing);

  auto* extent = document.NewElement("wp:extent");
  extent->SetAttribute("cx", width_emu.c_str());
  extent->SetAttribute("cy", height_emu.c_str());
  inline_drawing->InsertEndChild(extent);

  auto* doc_pr = document.NewElement("wp:docPr");
  doc_pr->SetAttribute("id", drawing_id_str.c_str());
  doc_pr->SetAttribute("name", image.file_name.c_str());
  inline_drawing->InsertEndChild(doc_pr);

  AppendImageGraphic(document, inline_drawing, image, *drawing_id);
}

void AppendTightParagraphProperties(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* paragraph) {
  auto* paragraph_properties = document.NewElement("w:pPr");
  paragraph->InsertEndChild(paragraph_properties);

  auto* spacing = document.NewElement("w:spacing");
  spacing->SetAttribute("w:before", "0");
  spacing->SetAttribute("w:after", "0");
  spacing->SetAttribute("w:line", "240");
  spacing->SetAttribute("w:lineRule", "auto");
  paragraph_properties->InsertEndChild(spacing);
}

bool IsRightAttachedPunctuation(char ch) {
  switch (ch) {
    case '.':
    case ',':
    case ';':
    case ':':
    case '!':
    case '?':
    case ')':
    case ']':
    case '}':
    case '%':
      return true;
    default:
      return false;
  }
}

bool IsLeftAttachedBracket(char ch) {
  switch (ch) {
    case '(':
    case '[':
    case '{':
      return true;
    default:
      return false;
  }
}

bool ContainsAnySubstring(const std::string& text,
                          const std::initializer_list<const char*>& tokens);

bool ContainsMathSymbol(const std::string& text) {
  return ContainsAnySubstring(
      text,
      {"∑", "∫", "√", "≤", "≥", "≈", "≠", "∞", "±", "×", "÷", "∀", "∃", "∈", "∩", "∪", "→", "↦", "∂"});
}

double SpanBaselineY(const ir::TextSpan& span) {
  if (span.y > 0.0) {
    return span.y;
  }
  if (span.has_bbox) {
    return span.bbox.y;
  }
  return SpanLineY(span);
}

double SpanHeight(const ir::TextSpan& span) {
  if (span.has_bbox && span.bbox.height > 0.0) {
    return span.bbox.height;
  }
  return 12.0;
}

template <typename T>
double MedianValue(std::vector<T> values) {
  if (values.empty()) {
    return 0.0;
  }
  std::sort(values.begin(), values.end());
  const size_t middle = values.size() / 2;
  if (values.size() % 2 == 1) {
    return static_cast<double>(values[middle]);
  }
  return 0.5 * (static_cast<double>(values[middle - 1]) + static_cast<double>(values[middle]));
}

std::vector<const ir::TextSpan*> SortLineSpansForReading(const std::vector<const ir::TextSpan*>& line_spans) {
  std::vector<const ir::TextSpan*> ordered = line_spans;
  if (ordered.size() <= 1) {
    return ordered;
  }

  std::vector<double> baselines;
  baselines.reserve(ordered.size());
  for (const auto* span : ordered) {
    if (span == nullptr) {
      continue;
    }
    baselines.push_back(SpanBaselineY(*span));
  }
  const double line_baseline = MedianValue(std::move(baselines));

  std::stable_sort(ordered.begin(), ordered.end(), [line_baseline](const ir::TextSpan* lhs, const ir::TextSpan* rhs) {
    if (lhs == nullptr || rhs == nullptr) {
      return lhs < rhs;
    }
    const double lhs_left = SpanLeftX(*lhs);
    const double rhs_left = SpanLeftX(*rhs);
    if (std::fabs(lhs_left - rhs_left) > 0.5) {
      return lhs_left < rhs_left;
    }

    const double lhs_baseline_distance = std::fabs(SpanBaselineY(*lhs) - line_baseline);
    const double rhs_baseline_distance = std::fabs(SpanBaselineY(*rhs) - line_baseline);
    if (std::fabs(lhs_baseline_distance - rhs_baseline_distance) > 0.2) {
      return lhs_baseline_distance < rhs_baseline_distance;
    }
    return SpanBaselineY(*lhs) > SpanBaselineY(*rhs);
  });
  return ordered;
}

std::string BuildLineText(const std::vector<const ir::TextSpan*>& ordered_line_spans) {
  std::string line_text;
  const ir::TextSpan* previous = nullptr;
  for (const auto* span : ordered_line_spans) {
    if (span == nullptr || span->text.empty()) {
      continue;
    }
    if (previous != nullptr && !line_text.empty()) {
      const double gap_pt = SpanLeftX(*span) - SpanRightX(*previous);
      const bool need_space =
          gap_pt > 1.0 &&
          !std::isspace(static_cast<unsigned char>(line_text.back())) &&
          !std::isspace(static_cast<unsigned char>(span->text.front())) &&
          !IsRightAttachedPunctuation(span->text.front()) &&
          !IsLeftAttachedBracket(line_text.back());
      if (need_space) {
        line_text.push_back(' ');
      }
    }
    line_text += span->text;
    previous = span;
  }
  return line_text;
}

enum class SpanScriptRole {
  kNormal,
  kSuperscript,
  kSubscript,
};

SpanScriptRole DetectScriptRole(const ir::TextSpan& span, double line_baseline, double script_threshold) {
  const double baseline = SpanBaselineY(span);
  if (baseline > line_baseline + script_threshold) {
    return SpanScriptRole::kSuperscript;
  }
  if (baseline < line_baseline - script_threshold) {
    return SpanScriptRole::kSubscript;
  }
  return SpanScriptRole::kNormal;
}

struct MathLineCandidate {
  std::string linear_text;
  bool has_script = false;
};

bool IsScriptLikeSpan(const ir::TextSpan& span) {
  if (span.text.empty()) {
    return false;
  }
  double effective_height = SpanHeight(span);
  if (span.has_bbox && span.bbox.height <= 0.0) {
    effective_height = 8.0;
  }
  if (effective_height > 10.0) {
    return false;
  }
  if (span.text.size() > 3) {
    return false;
  }
  for (unsigned char ch : span.text) {
    if (ch >= 128) {
      return false;
    }
    if (!std::isalnum(ch) && ch != '+' && ch != '-' && ch != '=' && ch != '(' && ch != ')' && ch != ',') {
      return false;
    }
  }
  return true;
}

bool SpanHasAsciiAlnum(const ir::TextSpan& span) {
  for (unsigned char ch : span.text) {
    if (ch < 128 && std::isalnum(ch)) {
      return true;
    }
  }
  return false;
}

bool IsBaselineAnchorSpan(const ir::TextSpan& span) {
  if (span.text.empty()) {
    return false;
  }
  if (IsScriptLikeSpan(span)) {
    return false;
  }
  if (SpanHasAsciiAlnum(span)) {
    return true;
  }
  if (ContainsMathSymbol(span.text)) {
    return false;
  }
  int punctuation_count = 0;
  for (unsigned char ch : span.text) {
    if (ch >= 128) {
      continue;
    }
    if (std::isalnum(ch)) {
      return true;
    }
    if (std::ispunct(ch)) {
      ++punctuation_count;
    }
  }
  return punctuation_count == 0;
}

struct LineEnvelope {
  double left = 0.0;
  double right = 0.0;
  bool has_any = false;
  bool has_math_symbol = false;
  int token_count = 0;
  int script_like_count = 0;
  int non_script_count = 0;
};

LineEnvelope ComputeLineEnvelope(const std::vector<const ir::TextSpan*>& line_spans) {
  LineEnvelope envelope;
  for (const auto* span : line_spans) {
    if (span == nullptr || span->text.empty()) {
      continue;
    }
    const double left = SpanLeftX(*span);
    const double right = SpanRightX(*span);
    if (!envelope.has_any) {
      envelope.left = left;
      envelope.right = right;
      envelope.has_any = true;
    } else {
      envelope.left = std::min(envelope.left, left);
      envelope.right = std::max(envelope.right, right);
    }
    ++envelope.token_count;
    if (IsScriptLikeSpan(*span)) {
      ++envelope.script_like_count;
    } else {
      ++envelope.non_script_count;
    }
    if (ContainsMathSymbol(span->text)) {
      envelope.has_math_symbol = true;
    }
  }
  return envelope;
}

double HorizontalGapToEnvelope(const LineEnvelope& envelope, const ir::TextSpan& span) {
  if (!envelope.has_any) {
    return std::numeric_limits<double>::infinity();
  }
  const double left = SpanLeftX(span);
  const double right = SpanRightX(span);
  if (right < envelope.left) {
    return envelope.left - right;
  }
  if (left > envelope.right) {
    return left - envelope.right;
  }
  return 0.0;
}

double ComputeLineBaseline(const std::vector<const ir::TextSpan*>& line_spans) {
  std::vector<double> normal_baselines;
  std::vector<double> all_baselines;
  normal_baselines.reserve(line_spans.size());
  all_baselines.reserve(line_spans.size());
  for (const auto* span : line_spans) {
    if (span == nullptr || span->text.empty()) {
      continue;
    }
    const double baseline = SpanBaselineY(*span);
    all_baselines.push_back(baseline);
    if (!IsScriptLikeSpan(*span)) {
      normal_baselines.push_back(baseline);
    }
  }
  if (!normal_baselines.empty()) {
    return MedianValue(std::move(normal_baselines));
  }
  return MedianValue(std::move(all_baselines));
}

bool LooksMathToken(const std::string& text) {
  if (text.empty()) {
    return false;
  }
  if (ContainsMathSymbol(text)) {
    return true;
  }
  if (ContainsAnySubstring(text, {"α", "β", "γ", "δ", "θ", "λ", "μ", "π", "σ", "ω", "Ω", "Δ"})) {
    return true;
  }
  if (text == "lim" ||
      ContainsAnySubstring(text, {"lim(", "lim_", "lim ", "sin(", "cos(", "tan(", "log(", "ln("})) {
    return true;
  }

  int operator_count = 0;
  int digit_count = 0;
  int alpha_count = 0;
  for (unsigned char ch : text) {
    if (ch < 128 && std::isdigit(ch)) {
      ++digit_count;
    } else if (ch < 128 && std::isalpha(ch)) {
      ++alpha_count;
    }
    switch (ch) {
      case '=':
      case '+':
      case '*':
      case '/':
      case '^':
      case '_':
      case '<':
      case '>':
        ++operator_count;
        break;
      default:
        break;
    }
  }
  if (operator_count == 0) {
    return false;
  }
  if (operator_count >= 2) {
    return true;
  }
  if (digit_count > 0 || alpha_count > 0) {
    return true;
  }
  if (text.size() == 1 && std::string("=+*/<>").find(text.front()) != std::string::npos) {
    return true;
  }
  return false;
}

MathLineCandidate BuildMathLinearText(const std::vector<const ir::TextSpan*>& ordered_line_spans) {
  MathLineCandidate candidate;
  if (ordered_line_spans.empty()) {
    return candidate;
  }

  std::vector<double> anchor_baselines;
  std::vector<double> baselines;
  std::vector<double> heights;
  anchor_baselines.reserve(ordered_line_spans.size());
  baselines.reserve(ordered_line_spans.size());
  heights.reserve(ordered_line_spans.size());
  for (const auto* span : ordered_line_spans) {
    if (span == nullptr) {
      continue;
    }
    const double span_baseline = SpanBaselineY(*span);
    baselines.push_back(span_baseline);
    if (IsBaselineAnchorSpan(*span)) {
      anchor_baselines.push_back(span_baseline);
    }
    heights.push_back(SpanHeight(*span));
  }
  const double line_baseline = !anchor_baselines.empty()
                                   ? MedianValue(std::move(anchor_baselines))
                                   : MedianValue(std::move(baselines));
  const double median_height = std::max(6.0, MedianValue(std::move(heights)));
  const double script_threshold = std::max(1.8, median_height * 0.28);

  const ir::TextSpan* previous = nullptr;
  SpanScriptRole previous_role = SpanScriptRole::kNormal;
  for (const auto* span : ordered_line_spans) {
    if (span == nullptr || span->text.empty()) {
      continue;
    }

    SpanScriptRole role = DetectScriptRole(*span, line_baseline, script_threshold);
    if (candidate.linear_text.empty()) {
      role = SpanScriptRole::kNormal;
    }
    if (role != SpanScriptRole::kNormal) {
      candidate.has_script = true;
    }

    if (role == SpanScriptRole::kNormal) {
      if (previous != nullptr && !candidate.linear_text.empty()) {
        const double gap_pt = SpanLeftX(*span) - SpanRightX(*previous);
        const bool need_space =
            gap_pt > 1.0 &&
            !std::isspace(static_cast<unsigned char>(candidate.linear_text.back())) &&
            !std::isspace(static_cast<unsigned char>(span->text.front())) &&
            !IsRightAttachedPunctuation(span->text.front()) &&
            !IsLeftAttachedBracket(candidate.linear_text.back()) &&
            previous_role == SpanScriptRole::kNormal;
        if (need_space) {
          candidate.linear_text.push_back(' ');
        }
      }
      candidate.linear_text += span->text;
    } else {
      while (!candidate.linear_text.empty() &&
             std::isspace(static_cast<unsigned char>(candidate.linear_text.back()))) {
        candidate.linear_text.pop_back();
      }
      candidate.linear_text += (role == SpanScriptRole::kSuperscript) ? "^" : "_";
      candidate.linear_text += span->text;
    }

    previous = span;
    previous_role = role;
  }
  return candidate;
}

bool ContainsAnySubstring(const std::string& text,
                          const std::initializer_list<const char*>& tokens) {
  for (const char* token : tokens) {
    if (token != nullptr && text.find(token) != std::string::npos) {
      return true;
    }
  }
  return false;
}

bool IsLikelyMathLine(const std::string& plain_text, const MathLineCandidate& candidate) {
  if (candidate.linear_text.empty()) {
    return false;
  }
  const std::string& text = candidate.linear_text;
  const bool has_unicode_math_symbol = ContainsMathSymbol(text);
  const bool has_greek_symbol =
      ContainsAnySubstring(text, {"α", "β", "γ", "δ", "θ", "λ", "μ", "π", "σ", "ω", "Ω", "Δ"});

  int operator_count = 0;
  int digit_count = 0;
  int alpha_count = 0;
  for (unsigned char ch : text) {
    if (ch < 128 && std::isdigit(ch)) {
      ++digit_count;
    }
    if (ch < 128 && std::isalpha(ch)) {
      ++alpha_count;
    }
    switch (ch) {
      case '=':
      case '+':
      case '-':
      case '*':
      case '/':
      case '^':
      case '_':
      case '<':
      case '>':
        ++operator_count;
        break;
      default:
        break;
    }
  }

  if (candidate.has_script &&
      (alpha_count > 0 || has_greek_symbol) &&
      (digit_count > 0 || operator_count > 0 || has_unicode_math_symbol)) {
    return true;
  }

  if (has_unicode_math_symbol && (digit_count > 0 || alpha_count > 0 || has_greek_symbol)) {
    return true;
  }

  if (has_unicode_math_symbol && text.size() <= 96) {
    return true;
  }

  if ((text.find('=') != std::string::npos) &&
      (alpha_count >= 2 || has_greek_symbol) &&
      text.size() <= 64) {
    return true;
  }

  if (operator_count >= 1 && digit_count > 0 && alpha_count > 0) {
    return true;
  }
  if (operator_count >= 2 && digit_count > 0) {
    return true;
  }
  if ((text.find('/') != std::string::npos || text.find("÷") != std::string::npos) &&
      (digit_count > 0 || alpha_count > 0 || has_greek_symbol) &&
      text.size() <= 40) {
    return true;
  }
  if (ContainsAnySubstring(text, {"sin(", "cos(", "tan(", "log(", "ln(", "lim"})) {
    return true;
  }

  if (plain_text.size() > 140 && operator_count <= 1) {
    return false;
  }
  return false;
}

enum class MathNodeKind {
  kToken,
  kRow,
  kFraction,
  kSup,
  kSub,
  kSubSup,
};

struct MathNode {
  MathNodeKind kind = MathNodeKind::kToken;
  std::string text;
  std::vector<MathNode> children;
};

MathNode MakeTokenNode(std::string text) {
  MathNode node;
  node.kind = MathNodeKind::kToken;
  node.text = std::move(text);
  return node;
}

MathNode MakeRowNode(std::vector<MathNode> children) {
  MathNode node;
  node.kind = MathNodeKind::kRow;
  node.children = std::move(children);
  return node;
}

MathNode MakeFractionNode(MathNode numerator, MathNode denominator) {
  MathNode node;
  node.kind = MathNodeKind::kFraction;
  node.children.push_back(std::move(numerator));
  node.children.push_back(std::move(denominator));
  return node;
}

MathNode MakeSupNode(MathNode base, MathNode superscript) {
  MathNode node;
  node.kind = MathNodeKind::kSup;
  node.children.push_back(std::move(base));
  node.children.push_back(std::move(superscript));
  return node;
}

MathNode MakeSubNode(MathNode base, MathNode subscript) {
  MathNode node;
  node.kind = MathNodeKind::kSub;
  node.children.push_back(std::move(base));
  node.children.push_back(std::move(subscript));
  return node;
}

MathNode MakeSubSupNode(MathNode base, MathNode subscript, MathNode superscript) {
  MathNode node;
  node.kind = MathNodeKind::kSubSup;
  node.children.push_back(std::move(base));
  node.children.push_back(std::move(subscript));
  node.children.push_back(std::move(superscript));
  return node;
}

MathNode MergeWithOperator(MathNode lhs, const std::string& op, MathNode rhs) {
  std::vector<MathNode> children;
  if (lhs.kind == MathNodeKind::kRow) {
    children = std::move(lhs.children);
  } else {
    children.push_back(std::move(lhs));
  }
  if (!op.empty()) {
    children.push_back(MakeTokenNode(op));
  }
  if (rhs.kind == MathNodeKind::kRow) {
    for (auto& child : rhs.children) {
      children.push_back(std::move(child));
    }
  } else {
    children.push_back(std::move(rhs));
  }
  return MakeRowNode(std::move(children));
}

enum class MathTokenType {
  kText,
  kOperator,
  kLParen,
  kRParen,
  kEnd,
};

struct MathToken {
  MathTokenType type = MathTokenType::kEnd;
  std::string text;
};

bool IsMathOperatorToken(const std::string& token) {
  return token == "+" || token == "-" || token == "−" ||
         token == "*" || token == "×" || token == "·" ||
         token == "/" || token == "÷" ||
         token == "=" || token == "<" || token == ">" ||
         token == "≤" || token == "≥" || token == "≈" || token == "≠" ||
         token == "^" || token == "_";
}

bool TokenHasAsciiAlnum(const std::string& token) {
  for (unsigned char ch : token) {
    if (ch < 128 && std::isalnum(ch)) {
      return true;
    }
  }
  return false;
}

bool TokenHasGreekSymbol(const std::string& token) {
  return ContainsAnySubstring(token, {"α", "β", "γ", "δ", "θ", "λ", "μ", "π", "σ", "ω", "Ω", "Δ"});
}

size_t Utf8CharLength(unsigned char lead) {
  if ((lead & 0x80u) == 0u) {
    return 1;
  }
  if ((lead & 0xE0u) == 0xC0u) {
    return 2;
  }
  if ((lead & 0xF0u) == 0xE0u) {
    return 3;
  }
  if ((lead & 0xF8u) == 0xF0u) {
    return 4;
  }
  return 1;
}

std::vector<MathToken> TokenizeMathLinearText(const std::string& text) {
  std::vector<MathToken> tokens;
  size_t index = 0;
  while (index < text.size()) {
    const unsigned char ch = static_cast<unsigned char>(text[index]);
    if (std::isspace(ch)) {
      ++index;
      continue;
    }

    if ((ch & 0x80u) != 0u) {
      const size_t codepoint_len = std::min(Utf8CharLength(ch), text.size() - index);
      std::string token_text = text.substr(index, codepoint_len);
      MathToken token;
      token.type = IsMathOperatorToken(token_text) ? MathTokenType::kOperator : MathTokenType::kText;
      token.text = std::move(token_text);
      tokens.push_back(std::move(token));
      index += codepoint_len;
      continue;
    }

    if (std::isalnum(ch) || ch == '.') {
      size_t end = index + 1;
      while (end < text.size()) {
        const unsigned char next = static_cast<unsigned char>(text[end]);
        if (!std::isalnum(next) && next != '.') {
          break;
        }
        ++end;
      }
      tokens.push_back(MathToken{
          .type = MathTokenType::kText,
          .text = text.substr(index, end - index),
      });
      index = end;
      continue;
    }

    const char ascii = static_cast<char>(ch);
    if (ascii == '(' || ascii == '[' || ascii == '{') {
      tokens.push_back(MathToken{
          .type = MathTokenType::kLParen,
          .text = "(",
      });
      ++index;
      continue;
    }
    if (ascii == ')' || ascii == ']' || ascii == '}') {
      tokens.push_back(MathToken{
          .type = MathTokenType::kRParen,
          .text = ")",
      });
      ++index;
      continue;
    }

    std::string token_text(1, ascii);
    tokens.push_back(MathToken{
        .type = IsMathOperatorToken(token_text) ? MathTokenType::kOperator : MathTokenType::kText,
        .text = std::move(token_text),
    });
    ++index;
  }

  tokens.push_back(MathToken{
      .type = MathTokenType::kEnd,
      .text = "",
  });
  return tokens;
}

class MathExpressionParser {
 public:
  explicit MathExpressionParser(std::vector<MathToken> tokens) : tokens_(std::move(tokens)) {}

  MathNode Parse() {
    MathNode parsed = ParseExpression();
    std::vector<MathNode> row;
    const auto append_node = [&row](MathNode node) {
      if (node.kind == MathNodeKind::kToken && node.text.empty()) {
        return;
      }
      if (node.kind == MathNodeKind::kRow) {
        for (auto& child : node.children) {
          if (child.kind == MathNodeKind::kToken && child.text.empty()) {
            continue;
          }
          row.push_back(std::move(child));
        }
        return;
      }
      row.push_back(std::move(node));
    };
    append_node(std::move(parsed));

    while (Peek().type != MathTokenType::kEnd) {
      if (Peek().type == MathTokenType::kLParen) {
        Consume();
        row.push_back(MakeTokenNode("("));
        continue;
      }
      if (Peek().type == MathTokenType::kRParen) {
        Consume();
        row.push_back(MakeTokenNode(")"));
        continue;
      }
      row.push_back(MakeTokenNode(Consume().text));
    }

    if (row.empty()) {
      return MakeTokenNode("0");
    }
    if (row.size() == 1) {
      return std::move(row.front());
    }
    return MakeRowNode(std::move(row));
  }

 private:
  const MathToken& Peek() const {
    return tokens_[cursor_];
  }

  const MathToken& Consume() {
    const size_t current = cursor_;
    if (cursor_ + 1 < tokens_.size()) {
      ++cursor_;
    }
    return tokens_[current];
  }

  bool Match(MathTokenType type, const std::string& text = "") {
    if (Peek().type != type) {
      return false;
    }
    if (!text.empty() && Peek().text != text) {
      return false;
    }
    Consume();
    return true;
  }

  bool MatchOperator(const std::initializer_list<const char*>& values, std::string* matched_value = nullptr) {
    if (Peek().type != MathTokenType::kOperator) {
      return false;
    }
    for (const char* value : values) {
      if (value != nullptr && Peek().text == value) {
        if (matched_value != nullptr) {
          *matched_value = Peek().text;
        }
        Consume();
        return true;
      }
    }
    return false;
  }

  MathNode ParseExpression() {
    MathNode node = ParseTerm();
    std::string op;
    while (MatchOperator({"+", "-", "−", "=", "<", ">", "≤", "≥", "≈", "≠"}, &op)) {
      MathNode rhs = ParseTerm();
      node = MergeWithOperator(std::move(node), op, std::move(rhs));
    }
    return node;
  }

  MathNode ParseTerm() {
    MathNode node = ParsePostfix();
    std::string op;
    while (MatchOperator({"*", "×", "·", "/", "÷"}, &op)) {
      MathNode rhs = ParsePostfix();
      if (op == "/" || op == "÷") {
        node = MakeFractionNode(std::move(node), std::move(rhs));
      } else {
        node = MergeWithOperator(std::move(node), op, std::move(rhs));
      }
    }
    return node;
  }

  MathNode ParsePostfix() {
    MathNode node = ParsePrimary();
    while (true) {
      if (Match(MathTokenType::kOperator, "^")) {
        MathNode script = ParsePrimary();
        if (node.kind == MathNodeKind::kSub && node.children.size() == 2) {
          node = MakeSubSupNode(std::move(node.children[0]), std::move(node.children[1]), std::move(script));
        } else {
          node = MakeSupNode(std::move(node), std::move(script));
        }
        continue;
      }
      if (Match(MathTokenType::kOperator, "_")) {
        MathNode script = ParsePrimary();
        if (node.kind == MathNodeKind::kSup && node.children.size() == 2) {
          node = MakeSubSupNode(std::move(node.children[0]), std::move(script), std::move(node.children[1]));
        } else {
          node = MakeSubNode(std::move(node), std::move(script));
        }
        continue;
      }
      break;
    }
    return node;
  }

  MathNode ParsePrimary() {
    if (Match(MathTokenType::kLParen)) {
      MathNode inner = ParseExpression();
      const bool has_closing = Match(MathTokenType::kRParen);
      std::vector<MathNode> grouped;
      grouped.push_back(MakeTokenNode("("));
      grouped.push_back(std::move(inner));
      if (has_closing) {
        grouped.push_back(MakeTokenNode(")"));
      }
      return MakeRowNode(std::move(grouped));
    }

    if (Peek().type == MathTokenType::kOperator &&
        (Peek().text == "+" || Peek().text == "-" || Peek().text == "−")) {
      const std::string unary = Consume().text;
      MathNode rhs = ParsePrimary();
      return MergeWithOperator(MakeTokenNode(unary), "", std::move(rhs));
    }

    if (Peek().type == MathTokenType::kText || Peek().type == MathTokenType::kOperator) {
      return MakeTokenNode(Consume().text);
    }

    if (Peek().type == MathTokenType::kRParen) {
      return MakeTokenNode(Consume().text);
    }

    return MakeTokenNode("");
  }

  std::vector<MathToken> tokens_;
  size_t cursor_ = 0;
};

void AppendMathNode(tinyxml2::XMLDocument& document,
                    tinyxml2::XMLElement* parent,
                    const MathNode& node) {
  if (parent == nullptr) {
    return;
  }

  switch (node.kind) {
    case MathNodeKind::kToken: {
      if (node.text.empty()) {
        return;
      }
      auto* run = document.NewElement("m:r");
      auto* text = document.NewElement("m:t");
      text->SetText(node.text.c_str());
      run->InsertEndChild(text);
      parent->InsertEndChild(run);
      return;
    }
    case MathNodeKind::kRow: {
      for (const auto& child : node.children) {
        AppendMathNode(document, parent, child);
      }
      return;
    }
    case MathNodeKind::kFraction: {
      if (node.children.size() != 2) {
        return;
      }
      auto* fraction = document.NewElement("m:f");
      parent->InsertEndChild(fraction);
      auto* numerator = document.NewElement("m:num");
      fraction->InsertEndChild(numerator);
      AppendMathNode(document, numerator, node.children[0]);
      auto* denominator = document.NewElement("m:den");
      fraction->InsertEndChild(denominator);
      AppendMathNode(document, denominator, node.children[1]);
      return;
    }
    case MathNodeKind::kSup: {
      if (node.children.size() != 2) {
        return;
      }
      auto* sup = document.NewElement("m:sSup");
      parent->InsertEndChild(sup);
      auto* base = document.NewElement("m:e");
      sup->InsertEndChild(base);
      AppendMathNode(document, base, node.children[0]);
      auto* superscript = document.NewElement("m:sup");
      sup->InsertEndChild(superscript);
      AppendMathNode(document, superscript, node.children[1]);
      return;
    }
    case MathNodeKind::kSub: {
      if (node.children.size() != 2) {
        return;
      }
      auto* sub = document.NewElement("m:sSub");
      parent->InsertEndChild(sub);
      auto* base = document.NewElement("m:e");
      sub->InsertEndChild(base);
      AppendMathNode(document, base, node.children[0]);
      auto* subscript = document.NewElement("m:sub");
      sub->InsertEndChild(subscript);
      AppendMathNode(document, subscript, node.children[1]);
      return;
    }
    case MathNodeKind::kSubSup: {
      if (node.children.size() != 3) {
        return;
      }
      auto* sub_sup = document.NewElement("m:sSubSup");
      parent->InsertEndChild(sub_sup);
      auto* base = document.NewElement("m:e");
      sub_sup->InsertEndChild(base);
      AppendMathNode(document, base, node.children[0]);
      auto* subscript = document.NewElement("m:sub");
      sub_sup->InsertEndChild(subscript);
      AppendMathNode(document, subscript, node.children[1]);
      auto* superscript = document.NewElement("m:sup");
      sub_sup->InsertEndChild(superscript);
      AppendMathNode(document, superscript, node.children[2]);
      return;
    }
  }
}

struct MathNodeStats {
  int token_count = 0;
  int operator_token_count = 0;
  int ascii_alnum_token_count = 0;
  int greek_identifier_token_count = 0;
  bool has_structured_math = false;
};

bool SubtreeHasMeaningfulIdentifier(const MathNode& node) {
  if (node.kind == MathNodeKind::kToken) {
    return TokenHasAsciiAlnum(node.text) || TokenHasGreekSymbol(node.text);
  }
  for (const auto& child : node.children) {
    if (SubtreeHasMeaningfulIdentifier(child)) {
      return true;
    }
  }
  return false;
}

void CollectMathNodeStats(const MathNode& node, MathNodeStats* stats) {
  if (stats == nullptr) {
    return;
  }

  switch (node.kind) {
    case MathNodeKind::kFraction:
      if (node.children.size() == 2 &&
          SubtreeHasMeaningfulIdentifier(node.children[0]) &&
          SubtreeHasMeaningfulIdentifier(node.children[1])) {
        stats->has_structured_math = true;
      }
      break;
    case MathNodeKind::kSup:
    case MathNodeKind::kSub:
      if (node.children.size() == 2 &&
          SubtreeHasMeaningfulIdentifier(node.children[0]) &&
          SubtreeHasMeaningfulIdentifier(node.children[1])) {
        stats->has_structured_math = true;
      }
      break;
    case MathNodeKind::kSubSup:
      if (node.children.size() == 3 &&
          SubtreeHasMeaningfulIdentifier(node.children[0]) &&
          SubtreeHasMeaningfulIdentifier(node.children[1]) &&
          SubtreeHasMeaningfulIdentifier(node.children[2])) {
        stats->has_structured_math = true;
      }
      break;
    default:
      break;
  }

  if (node.kind == MathNodeKind::kToken) {
    if (node.text.empty()) {
      return;
    }
    ++stats->token_count;
    if (IsMathOperatorToken(node.text)) {
      ++stats->operator_token_count;
    }
    if (TokenHasAsciiAlnum(node.text)) {
      ++stats->ascii_alnum_token_count;
    }
    if (TokenHasGreekSymbol(node.text)) {
      ++stats->greek_identifier_token_count;
    }
    return;
  }

  for (const auto& child : node.children) {
    CollectMathNodeStats(child, stats);
  }
}

void AppendTextParagraph(tinyxml2::XMLDocument& document,
                         tinyxml2::XMLElement* body,
                         const std::string& line_text) {
  if (line_text.empty()) {
    return;
  }
  auto* paragraph = document.NewElement("w:p");
  body->InsertEndChild(paragraph);
  AppendTightParagraphProperties(document, paragraph);

  auto* run = document.NewElement("w:r");
  paragraph->InsertEndChild(run);

  auto* text = document.NewElement("w:t");
  text->SetText(line_text.c_str());
  run->InsertEndChild(text);
}

bool AppendMathParagraph(tinyxml2::XMLDocument& document,
                         tinyxml2::XMLElement* body,
                         const std::string& linear_expression) {
  if (linear_expression.empty()) {
    return false;
  }

  MathExpressionParser parser(TokenizeMathLinearText(linear_expression));
  MathNode parsed = parser.Parse();
  MathNodeStats stats;
  CollectMathNodeStats(parsed, &stats);
  const int identifier_token_count = stats.ascii_alnum_token_count + stats.greek_identifier_token_count;
  const bool has_unicode_math_symbol = ContainsMathSymbol(linear_expression);
  const bool should_emit_math =
      stats.has_structured_math ||
      (stats.operator_token_count > 0 && identifier_token_count >= 2) ||
      (stats.operator_token_count > 0 && identifier_token_count >= 1 && has_unicode_math_symbol) ||
      (has_unicode_math_symbol && identifier_token_count >= 1);
  if (!should_emit_math) {
    return false;
  }

  auto* paragraph = document.NewElement("w:p");
  body->InsertEndChild(paragraph);
  AppendTightParagraphProperties(document, paragraph);

  auto* math_paragraph = document.NewElement("m:oMathPara");
  paragraph->InsertEndChild(math_paragraph);
  auto* math = document.NewElement("m:oMath");
  math_paragraph->InsertEndChild(math);
  AppendMathNode(document, math, parsed);
  return true;
}

void AppendPageTextParagraphs(tinyxml2::XMLDocument& document,
                              tinyxml2::XMLElement* body,
                              const ir::Page& page) {
  constexpr double kLineTolerancePt = 3.0;
  constexpr double kScriptAttachTolerancePt = 9.0;
  constexpr double kLooseScriptAttachTolerancePt = 20.0;
  constexpr double kScriptAttachMinGapPt = -2.0;
  constexpr double kScriptAttachMaxGapPt = 8.0;
  constexpr double kMathEnvelopeAttachMaxGapPt = 48.0;
  constexpr double kMathEnvelopeLooseGapPt = 24.0;
  std::vector<const ir::TextSpan*> current_line;
  double current_line_baseline = 0.0;
  bool has_current_line = false;

  const auto flush_line = [&]() {
    if (current_line.empty()) {
      return;
    }
    std::vector<const ir::TextSpan*> ordered_line_spans = SortLineSpansForReading(current_line);
    const std::string line_text = BuildLineText(ordered_line_spans);
    const MathLineCandidate math_line = BuildMathLinearText(ordered_line_spans);
    current_line.clear();
    has_current_line = false;

    if (line_text.empty()) {
      return;
    }

    if (IsLikelyMathLine(line_text, math_line) &&
        AppendMathParagraph(document, body, math_line.linear_text)) {
      return;
    } else {
      AppendTextParagraph(document, body, line_text);
    }
  };

  for (const auto& span : page.spans) {
    if (span.text.empty()) {
      continue;
    }
    const double span_baseline = SpanBaselineY(span);
    if (!has_current_line) {
      current_line.push_back(&span);
      current_line_baseline = span_baseline;
      has_current_line = true;
      continue;
    }

    const LineEnvelope current_envelope = ComputeLineEnvelope(current_line);
    const bool span_script_like = IsScriptLikeSpan(span);
    const bool span_math_like = LooksMathToken(span.text);
    const bool line_script_stub =
        current_envelope.script_like_count > 0 &&
        current_envelope.non_script_count == 0 &&
        current_envelope.token_count <= 2;
    const bool line_math_like =
        current_envelope.has_math_symbol ||
        current_envelope.script_like_count >= 2;

    bool same_line = std::fabs(current_line_baseline - span_baseline) <= kLineTolerancePt;
    if (!same_line && !current_line.empty()) {
      const auto* previous = current_line.back();
      if (previous != nullptr) {
        const double previous_baseline = SpanBaselineY(*previous);
        const double baseline_delta = std::fabs(previous_baseline - span_baseline);
        const double gap_pt = SpanLeftX(span) - SpanRightX(*previous);
        const bool close_glyph_attachment =
            baseline_delta <= kScriptAttachTolerancePt &&
            gap_pt >= kScriptAttachMinGapPt &&
            gap_pt <= kScriptAttachMaxGapPt;
        same_line = close_glyph_attachment;
      }
    }

    if (!same_line && current_envelope.has_any) {
      const double baseline_delta = std::fabs(current_line_baseline - span_baseline);
      const double envelope_gap = HorizontalGapToEnvelope(current_envelope, span);
      if (baseline_delta <= kScriptAttachTolerancePt &&
          envelope_gap <= kMathEnvelopeAttachMaxGapPt &&
          (line_math_like || span_script_like || span_math_like)) {
        same_line = true;
      }
      if (!same_line &&
          baseline_delta <= kLooseScriptAttachTolerancePt &&
          envelope_gap <= kMathEnvelopeAttachMaxGapPt &&
          span_script_like &&
          (line_math_like || span_math_like)) {
        same_line = true;
      }
      if (!same_line &&
          baseline_delta <= kLooseScriptAttachTolerancePt &&
          envelope_gap <= kMathEnvelopeLooseGapPt &&
          line_math_like &&
          span_math_like) {
        same_line = true;
      }
      if (!same_line &&
          baseline_delta <= kScriptAttachTolerancePt &&
          envelope_gap <= kMathEnvelopeLooseGapPt &&
          line_script_stub &&
          (span_script_like || span_math_like)) {
        same_line = true;
      }
    }

    if (same_line) {
      current_line.push_back(&span);
      current_line_baseline = ComputeLineBaseline(current_line);
      continue;
    }

    flush_line();
    current_line.push_back(&span);
    current_line_baseline = span_baseline;
    has_current_line = true;
  }
  flush_line();
}

std::string BuildDocumentXml(const ir::Document& ir_document,
                             const std::vector<DocxImageRef>& images,
                             bool use_anchored_images) {
  tinyxml2::XMLDocument document;
  tinyxml2::XMLNode* declaration = document.NewDeclaration();
  document.InsertFirstChild(declaration);

  auto* root = document.NewElement("w:document");
  root->SetAttribute("xmlns:w", "http://schemas.openxmlformats.org/wordprocessingml/2006/main");
  root->SetAttribute("xmlns:r", "http://schemas.openxmlformats.org/officeDocument/2006/relationships");
  root->SetAttribute("xmlns:m", "http://schemas.openxmlformats.org/officeDocument/2006/math");
  root->SetAttribute("xmlns:wp", "http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing");
  root->SetAttribute("xmlns:a", "http://schemas.openxmlformats.org/drawingml/2006/main");
  root->SetAttribute("xmlns:pic", "http://schemas.openxmlformats.org/drawingml/2006/picture");
  document.InsertEndChild(root);

  auto* body = document.NewElement("w:body");
  root->InsertEndChild(body);

  size_t image_cursor = 0;
  uint32_t drawing_id = 1;
  for (size_t page_index = 0; page_index < ir_document.pages.size(); ++page_index) {
    const auto& page = ir_document.pages[page_index];
    AppendPageTextParagraphs(document, body, page);

    while (image_cursor < images.size() && images[image_cursor].page_index < page_index) {
      ++image_cursor;
    }
    while (image_cursor < images.size() && images[image_cursor].page_index == page_index) {
      const auto& image = images[image_cursor];

      auto* paragraph = document.NewElement("w:p");
      body->InsertEndChild(paragraph);

      auto* run = document.NewElement("w:r");
      paragraph->InsertEndChild(run);

      AppendImageDrawing(document, run, image, use_anchored_images, &drawing_id);

      ++image_cursor;
    }

    auto* page_break_paragraph = document.NewElement("w:p");
    auto* page_break_run = document.NewElement("w:r");
    auto* page_break = document.NewElement("w:br");
    page_break->SetAttribute("w:type", "page");
    page_break_run->InsertEndChild(page_break);
    page_break_paragraph->InsertEndChild(page_break_run);
    body->InsertEndChild(page_break_paragraph);
  }

  auto* section = document.NewElement("w:sectPr");
  body->InsertEndChild(section);

  tinyxml2::XMLPrinter printer;
  document.Print(&printer);
  return printer.CStr();
}

std::string BuildStylesXml() {
  return R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<w:styles xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">
  <w:docDefaults>
    <w:rPrDefault>
      <w:rPr>
        <w:rFonts w:ascii="Calibri" w:hAnsi="Calibri" w:eastAsia="Calibri" w:cs="Calibri"/>
        <w:sz w:val="22"/>
        <w:szCs w:val="22"/>
      </w:rPr>
    </w:rPrDefault>
  </w:docDefaults>
</w:styles>)";
}
#endif

std::string BuildContentTypesXml(const std::vector<DocxImageRef>& images) {
  std::set<std::pair<std::string, std::string>> defaults = {
      {"rels", "application/vnd.openxmlformats-package.relationships+xml"},
      {"xml", "application/xml"},
  };

  for (const auto& image : images) {
    defaults.insert({image.extension, image.mime_type});
  }

  std::ostringstream stream;
  stream << R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
)";
  for (const auto& [extension, content_type] : defaults) {
    stream << "  <Default Extension=\"" << extension << "\" ContentType=\"" << content_type << "\"/>\n";
  }
  stream << "  <Override PartName=\"/word/document.xml\" "
            "ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>\n";
  stream << "  <Override PartName=\"/word/styles.xml\" "
            "ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml\"/>\n";
  stream << "</Types>";
  return stream.str();
}

std::string BuildRootRelsXml() {
  return R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/>
</Relationships>)";
}

std::string BuildDocumentRelsXml(const std::vector<DocxImageRef>& images) {
  std::ostringstream stream;
  stream << R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
)";
  stream << "  <Relationship Id=\"rIdStyles\" "
            "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" "
            "Target=\"styles.xml\"/>\n";
  for (const auto& image : images) {
    stream << "  <Relationship Id=\"" << image.relationship_id
           << "\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/image\" "
           << "Target=\"media/" << image.file_name << "\"/>\n";
  }
  stream << "</Relationships>";
  return stream.str();
}

#if PDF2DOCX_HAS_MINIZIP
Status AddZipEntry(zipFile archive, const char* entry_name, const void* data, size_t size) {
  zip_fileinfo file_info = {};
  int open_result = zipOpenNewFileInZip(
      archive,
      entry_name,
      &file_info,
      nullptr,
      0,
      nullptr,
      0,
      nullptr,
      Z_DEFLATED,
      Z_DEFAULT_COMPRESSION);
  if (open_result != ZIP_OK) {
    return Status::Error(ErrorCode::kIoError, "zipOpenNewFileInZip failed");
  }

  if (size > 0) {
    int write_result = zipWriteInFileInZip(archive, data, static_cast<unsigned int>(size));
    if (write_result != ZIP_OK) {
      zipCloseFileInZip(archive);
      return Status::Error(ErrorCode::kIoError, "zipWriteInFileInZip failed");
    }
  }

  int close_result = zipCloseFileInZip(archive);
  if (close_result != ZIP_OK) {
    return Status::Error(ErrorCode::kIoError, "zipCloseFileInZip failed");
  }

  return Status::Ok();
}

Status AddZipEntry(zipFile archive, const char* entry_name, const std::string& content) {
  return AddZipEntry(archive, entry_name, content.data(), content.size());
}
#endif

}  // namespace

Status P0Writer::WriteFromIr(const ir::Document& document,
                             const std::string& output_docx,
                             const ConvertStats& stats,
                             const DocxWriteOptions& options) const {
  if (output_docx.empty()) {
    return Status::Error(ErrorCode::kInvalidArgument, "output docx path is empty");
  }

#if PDF2DOCX_HAS_TINYXML2 && PDF2DOCX_HAS_MINIZIP
  const std::vector<DocxImageRef> docx_images = CollectDocxImages(document);
  const std::string document_xml = BuildDocumentXml(document, docx_images, options.use_anchored_images);
  const std::string styles_xml = BuildStylesXml();
  const std::string content_types_xml = BuildContentTypesXml(docx_images);
  const std::string rels_xml = BuildRootRelsXml();
  const std::string document_rels_xml = BuildDocumentRelsXml(docx_images);

  zipFile archive = zipOpen64(output_docx.c_str(), APPEND_STATUS_CREATE);
  if (archive == nullptr) {
    return Status::Error(ErrorCode::kIoError, "cannot create output docx zip");
  }

  Status add_content_types = AddZipEntry(archive, "[Content_Types].xml", content_types_xml);
  if (!add_content_types.ok()) {
    zipClose(archive, nullptr);
    return add_content_types;
  }

  Status add_root_rels = AddZipEntry(archive, "_rels/.rels", rels_xml);
  if (!add_root_rels.ok()) {
    zipClose(archive, nullptr);
    return add_root_rels;
  }

  Status add_document_xml = AddZipEntry(archive, "word/document.xml", document_xml);
  if (!add_document_xml.ok()) {
    zipClose(archive, nullptr);
    return add_document_xml;
  }

  Status add_styles_xml = AddZipEntry(archive, "word/styles.xml", styles_xml);
  if (!add_styles_xml.ok()) {
    zipClose(archive, nullptr);
    return add_styles_xml;
  }

  Status add_document_rels = AddZipEntry(archive, "word/_rels/document.xml.rels", document_rels_xml);
  if (!add_document_rels.ok()) {
    zipClose(archive, nullptr);
    return add_document_rels;
  }

  for (const auto& image : docx_images) {
    Status add_image = AddZipEntry(archive, image.part_name.c_str(), image.data.data(), image.data.size());
    if (!add_image.ok()) {
      zipClose(archive, nullptr);
      return add_image;
    }
  }

  if (zipClose(archive, nullptr) != ZIP_OK) {
    return Status::Error(ErrorCode::kIoError, "zipClose failed");
  }

  (void)stats;
  return Status::Ok();
#else
  std::ofstream stream(output_docx, std::ios::out | std::ios::trunc);
  if (!stream.is_open()) {
    return Status::Error(ErrorCode::kIoError, "cannot open output file");
  }

  stream << "pdf2docx P1 placeholder output\n";
  stream << "backend=" << stats.backend << "\n";
  stream << "xml_backend=" << stats.xml_backend << "\n";
  stream << "page_count=" << document.pages.size() << "\n";
  size_t span_count = 0;
  for (const auto& page : document.pages) {
    span_count += page.spans.size();
  }
  stream << "span_count=" << span_count << "\n";
  stream << "note=This is a framework placeholder; OOXML writer lands in next milestones.\n";
  stream.close();

  return Status::Ok();
#endif
}

}  // namespace pdf2docx::docx
