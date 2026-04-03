#include "pipeline/pipeline.hpp"

#include <algorithm>
#include <cmath>

namespace pdf2docx::pipeline {

namespace {

constexpr double kLineTolerancePt = 2.0;
constexpr double kColumnTolerancePt = 0.5;

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
  }

  if (stats != nullptr) {
    *stats = local_stats;
  }

  return Status::Ok();
}

}  // namespace pdf2docx::pipeline
