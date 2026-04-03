#include "pipeline/pipeline.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

namespace pdf2docx::pipeline {

namespace {

constexpr double kLineTolerancePt = 2.0;
constexpr double kColumnTolerancePt = 0.5;
constexpr double kMergeGapMaxPt = 8.0;
constexpr double kMergeOverlapTolerancePt = 0.75;
constexpr double kDefaultSpanWidthPt = 8.0;
constexpr double kDefaultSpanHeightPt = 12.0;

double SpanTopY(const ir::TextSpan& span) {
  if (span.has_bbox) {
    return span.bbox.y + span.bbox.height;
  }
  return span.y;
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
  return span.x + std::max(kDefaultSpanWidthPt, span.length);
}

ir::Rect SpanRect(const ir::TextSpan& span) {
  if (span.has_bbox) {
    return span.bbox;
  }
  return ir::Rect{
      .x = span.x,
      .y = span.y,
      .width = std::max(kDefaultSpanWidthPt, span.length),
      .height = kDefaultSpanHeightPt,
  };
}

bool IsLineAligned(const ir::TextSpan& lhs, const ir::TextSpan& rhs) {
  if (std::fabs(SpanTopY(lhs) - SpanTopY(rhs)) <= kLineTolerancePt) {
    return true;
  }
  return std::fabs(lhs.y - rhs.y) <= kLineTolerancePt;
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

bool ShouldInsertSpaceBetween(const std::string& left_text,
                              const std::string& right_text,
                              double gap_pt) {
  if (left_text.empty() || right_text.empty()) {
    return false;
  }
  if (std::isspace(static_cast<unsigned char>(left_text.back())) ||
      std::isspace(static_cast<unsigned char>(right_text.front()))) {
    return false;
  }
  if (gap_pt <= 0.75) {
    return false;
  }
  if (IsRightAttachedPunctuation(right_text.front())) {
    return false;
  }
  if (IsLeftAttachedBracket(left_text.back())) {
    return false;
  }
  return true;
}

bool TryMergeAdjacentSpan(ir::TextSpan* current, const ir::TextSpan& next) {
  if (current == nullptr || current->text.empty() || next.text.empty()) {
    return false;
  }
  if (!IsLineAligned(*current, next)) {
    return false;
  }

  const double gap_pt = SpanLeftX(next) - SpanRightX(*current);
  if (gap_pt < -kMergeOverlapTolerancePt || gap_pt > kMergeGapMaxPt) {
    return false;
  }

  const ir::Rect current_rect = SpanRect(*current);
  const ir::Rect next_rect = SpanRect(next);
  const double union_left = std::min(current_rect.x, next_rect.x);
  const double union_bottom = std::min(current_rect.y, next_rect.y);
  const double union_right = std::max(current_rect.x + current_rect.width, next_rect.x + next_rect.width);
  const double union_top = std::max(current_rect.y + current_rect.height, next_rect.y + next_rect.height);

  if (ShouldInsertSpaceBetween(current->text, next.text, gap_pt)) {
    current->text.push_back(' ');
  }
  current->text += next.text;
  current->x = union_left;
  current->y = union_bottom;
  current->length = std::max(1.0, union_right - union_left);
  current->has_bbox = true;
  current->bbox = ir::Rect{
      .x = union_left,
      .y = union_bottom,
      .width = std::max(1.0, union_right - union_left),
      .height = std::max(1.0, union_top - union_bottom),
  };
  return true;
}

bool SpanEqual(const ir::TextSpan& lhs, const ir::TextSpan& rhs) {
  return lhs.text == rhs.text &&
         lhs.x == rhs.x &&
         lhs.y == rhs.y &&
         lhs.length == rhs.length &&
         lhs.has_bbox == rhs.has_bbox &&
         lhs.bbox.x == rhs.bbox.x &&
         lhs.bbox.y == rhs.bbox.y &&
         lhs.bbox.width == rhs.bbox.width &&
         lhs.bbox.height == rhs.bbox.height;
}

}  // namespace

Status Pipeline::Execute(ir::Document* document, PipelineStats* stats) const {
  if (document == nullptr) {
    return Status::Error(ErrorCode::kInvalidArgument, "pipeline document is null");
  }

  PipelineStats local_stats{};

  for (auto& page : document->pages) {
    ++local_stats.page_count;
    if (page.spans.size() <= 1) {
      continue;
    }

    const auto original_spans = page.spans;
    std::stable_sort(page.spans.begin(), page.spans.end(), [](const ir::TextSpan& lhs, const ir::TextSpan& rhs) {
      const double lhs_top = SpanTopY(lhs);
      const double rhs_top = SpanTopY(rhs);
      if (std::fabs(lhs_top - rhs_top) > kLineTolerancePt) {
        // PDF origin is bottom-left, so larger y should come first for reading order.
        return lhs_top > rhs_top;
      }

      const double lhs_left = SpanLeftX(lhs);
      const double rhs_left = SpanLeftX(rhs);
      if (std::fabs(lhs_left - rhs_left) > kColumnTolerancePt) {
        return lhs_left < rhs_left;
      }
      return false;
    });

    bool page_reordered = false;
    for (size_t i = 0; i < page.spans.size(); ++i) {
      if (!SpanEqual(original_spans[i], page.spans[i])) {
        ++local_stats.reordered_span_count;
        page_reordered = true;
      }
    }
    if (page_reordered) {
      ++local_stats.reordered_page_count;
    }

    std::vector<ir::TextSpan> merged_spans;
    merged_spans.reserve(page.spans.size());
    for (const auto& span : page.spans) {
      if (merged_spans.empty()) {
        merged_spans.push_back(span);
        continue;
      }
      if (TryMergeAdjacentSpan(&merged_spans.back(), span)) {
        ++local_stats.merged_span_count;
      } else {
        merged_spans.push_back(span);
      }
    }
    page.spans = std::move(merged_spans);
  }

  if (stats != nullptr) {
    *stats = local_stats;
  }

  return Status::Ok();
}

}  // namespace pdf2docx::pipeline
