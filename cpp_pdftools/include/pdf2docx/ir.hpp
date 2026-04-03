#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pdf2docx::ir {

struct Rect {
  double x = 0.0;
  double y = 0.0;
  double width = 0.0;
  double height = 0.0;
};

struct Point {
  double x = 0.0;
  double y = 0.0;
};

struct Quad {
  Point p0{};
  Point p1{};
  Point p2{};
  Point p3{};
};

struct TextSpan {
  std::string text;
  double x = 0.0;
  double y = 0.0;
  double length = 0.0;
  bool has_bbox = false;
  Rect bbox{};
};

struct ImageBlock {
  std::string id;
  std::string mime_type;
  std::string extension;
  double x = 0.0;
  double y = 0.0;
  double width = 0.0;
  double height = 0.0;
  bool has_quad = false;
  Quad quad{};
  std::vector<uint8_t> data;
};

struct Page {
  double width_pt = 0.0;
  double height_pt = 0.0;
  std::vector<TextSpan> spans;
  std::vector<ImageBlock> images;
};

struct Document {
  std::vector<Page> pages;
};

}  // namespace pdf2docx::ir
