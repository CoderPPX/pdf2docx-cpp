#include "docx/p0_writer.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
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
};

int64_t PtToEmu(double pt) {
  constexpr double kEmuPerPt = 12700.0;
  return std::max<int64_t>(1, static_cast<int64_t>(std::llround(pt * kEmuPerPt)));
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
      ref.width_emu = PtToEmu(std::max(1.0, image.width));
      ref.height_emu = PtToEmu(std::max(1.0, image.height));
      refs.push_back(std::move(ref));
      ++index;
    }
  }

  return refs;
}

#if PDF2DOCX_HAS_TINYXML2
std::string BuildDocumentXml(const ir::Document& ir_document, const std::vector<DocxImageRef>& images) {
  tinyxml2::XMLDocument document;
  tinyxml2::XMLNode* declaration = document.NewDeclaration();
  document.InsertFirstChild(declaration);

  auto* root = document.NewElement("w:document");
  root->SetAttribute("xmlns:w", "http://schemas.openxmlformats.org/wordprocessingml/2006/main");
  root->SetAttribute("xmlns:r", "http://schemas.openxmlformats.org/officeDocument/2006/relationships");
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
    for (const auto& span : page.spans) {
      if (span.text.empty()) {
        continue;
      }

      auto* paragraph = document.NewElement("w:p");
      body->InsertEndChild(paragraph);

      auto* run = document.NewElement("w:r");
      paragraph->InsertEndChild(run);

      auto* text = document.NewElement("w:t");
      text->SetText(span.text.c_str());
      run->InsertEndChild(text);
    }

    while (image_cursor < images.size() && images[image_cursor].page_index < page_index) {
      ++image_cursor;
    }
    while (image_cursor < images.size() && images[image_cursor].page_index == page_index) {
      const auto& image = images[image_cursor];

      auto* paragraph = document.NewElement("w:p");
      body->InsertEndChild(paragraph);

      auto* run = document.NewElement("w:r");
      paragraph->InsertEndChild(run);

      auto* drawing = document.NewElement("w:drawing");
      run->InsertEndChild(drawing);

      auto* inline_drawing = document.NewElement("wp:inline");
      inline_drawing->SetAttribute("distT", "0");
      inline_drawing->SetAttribute("distB", "0");
      inline_drawing->SetAttribute("distL", "0");
      inline_drawing->SetAttribute("distR", "0");
      drawing->InsertEndChild(inline_drawing);

      auto* extent = document.NewElement("wp:extent");
      const std::string width_emu = std::to_string(image.width_emu);
      const std::string height_emu = std::to_string(image.height_emu);
      extent->SetAttribute("cx", width_emu.c_str());
      extent->SetAttribute("cy", height_emu.c_str());
      inline_drawing->InsertEndChild(extent);

      auto* doc_pr = document.NewElement("wp:docPr");
      const std::string drawing_id_str = std::to_string(drawing_id++);
      doc_pr->SetAttribute("id", drawing_id_str.c_str());
      doc_pr->SetAttribute("name", image.file_name.c_str());
      inline_drawing->InsertEndChild(doc_pr);

      auto* graphic = document.NewElement("a:graphic");
      inline_drawing->InsertEndChild(graphic);

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
      sp_pr->InsertEndChild(xfrm);

      auto* off = document.NewElement("a:off");
      off->SetAttribute("x", "0");
      off->SetAttribute("y", "0");
      xfrm->InsertEndChild(off);

      auto* ext = document.NewElement("a:ext");
      ext->SetAttribute("cx", width_emu.c_str());
      ext->SetAttribute("cy", height_emu.c_str());
      xfrm->InsertEndChild(ext);

      auto* prst_geom = document.NewElement("a:prstGeom");
      prst_geom->SetAttribute("prst", "rect");
      sp_pr->InsertEndChild(prst_geom);

      auto* av_lst = document.NewElement("a:avLst");
      prst_geom->InsertEndChild(av_lst);

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
                             const ConvertStats& stats) const {
  if (output_docx.empty()) {
    return Status::Error(ErrorCode::kInvalidArgument, "output docx path is empty");
  }

#if PDF2DOCX_HAS_TINYXML2 && PDF2DOCX_HAS_MINIZIP
  const std::vector<DocxImageRef> docx_images = CollectDocxImages(document);
  const std::string document_xml = BuildDocumentXml(document, docx_images);
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

  if (!docx_images.empty()) {
    Status add_document_rels = AddZipEntry(archive, "word/_rels/document.xml.rels", document_rels_xml);
    if (!add_document_rels.ok()) {
      zipClose(archive, nullptr);
      return add_document_rels;
    }
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
