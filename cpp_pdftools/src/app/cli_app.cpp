#include "pdftools/cli.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "pdftools/convert/pdf2docx.hpp"
#include "pdftools/pdf/create_ops.hpp"
#include "pdftools/pdf/document_ops.hpp"
#include "pdftools/pdf/extract_ops.hpp"

namespace pdftools {

namespace {

void PrintHelp(std::ostream& out) {
  out << "Usage:\n"
      << "  pdftools merge <output.pdf> <input1.pdf> <input2.pdf> [inputN.pdf...]\n"
      << "  pdftools text extract --input <in.pdf> --output <out.txt|out.json> [--json] [--strict]\n"
      << "  pdftools attachments extract --input <in.pdf> --out-dir <dir> [--strict]\n"
      << "  pdftools pdf info --input <in.pdf>\n"
      << "  pdftools image2pdf --output <out.pdf> --images <img1> <img2> [imgN]\n"
      << "  pdftools page delete --input <in.pdf> --output <out.pdf> --page <n>\n"
      << "  pdftools page insert --input <in.pdf> --output <out.pdf> --at <n> --source <src.pdf> --source-page <m>\n"
      << "  pdftools page replace --input <in.pdf> --output <out.pdf> --page <n> --source <src.pdf> --source-page <m>\n"
      << "  pdftools convert pdf2docx --input <in.pdf> --output <out.docx> [--dump-ir <ir.json>] [--no-images] [--docx-anchored]\n";
}

bool HasFlag(const std::vector<std::string>& args, const std::string& flag) {
  for (const auto& arg : args) {
    if (arg == flag) {
      return true;
    }
  }
  return false;
}

std::optional<std::string> GetOptionValue(const std::vector<std::string>& args, const std::string& key) {
  for (size_t i = 0; i + 1 < args.size(); ++i) {
    if (args[i] == key) {
      return args[i + 1];
    }
  }
  return std::nullopt;
}

bool ParseUint32(const std::string& value, uint32_t* out) {
  if (out == nullptr || value.empty()) {
    return false;
  }
  char* end = nullptr;
  const unsigned long parsed = std::strtoul(value.c_str(), &end, 10);
  if (end == value.c_str() || *end != '\0') {
    return false;
  }
  if (parsed > 0xFFFFFFFFUL) {
    return false;
  }
  *out = static_cast<uint32_t>(parsed);
  return true;
}

int PrintStatus(std::ostream& err, const Status& status) {
  err << "Error: " << status.message();
  if (!status.context().empty()) {
    err << " (" << status.context() << ")";
  }
  err << "\n";
  if (status.code() == ErrorCode::kInvalidArgument) {
    return 1;
  }
  return 2;
}

}  // namespace

int RunCli(const std::vector<std::string>& args, std::ostream& out, std::ostream& err) {
  if (args.size() < 2 || args[1] == "--help" || args[1] == "-h") {
    PrintHelp(out);
    return args.size() < 2 ? 1 : 0;
  }

  const std::string& command = args[1];

  if (command == "merge") {
    if (args.size() < 5) {
      PrintHelp(err);
      return 1;
    }
    pdf::MergePdfRequest request;
    request.output_pdf = args[2];
    request.input_pdfs.assign(args.begin() + 3, args.end());

    pdf::MergePdfResult result;
    Status status = pdf::MergePdf(request, &result);
    if (!status.ok()) {
      return PrintStatus(err, status);
    }
    out << "merged pages=" << result.output_page_count << "\n";
    return 0;
  }

  if (command == "text" && args.size() >= 3 && args[2] == "extract") {
    pdf::ExtractTextRequest request;
    auto input = GetOptionValue(args, "--input");
    auto output = GetOptionValue(args, "--output");
    if (!input.has_value() || !output.has_value()) {
      err << "text extract requires --input and --output\n";
      return 1;
    }
    request.input_pdf = *input;
    request.output_path = *output;
    request.include_positions = HasFlag(args, "--include-positions");
    request.output_format = HasFlag(args, "--json") ? pdf::TextOutputFormat::kJson : pdf::TextOutputFormat::kPlainText;
    request.best_effort = !HasFlag(args, "--strict");

    pdf::ExtractTextResult result;
    Status status = pdf::ExtractText(request, &result);
    if (!status.ok()) {
      return PrintStatus(err, status);
    }
    out << "text extracted pages=" << result.page_count
        << " entries=" << result.entry_count
        << " extractor=" << result.extractor
        << " fallback=" << (result.used_fallback ? "yes" : "no") << "\n";
    return 0;
  }

  if (command == "attachments" && args.size() >= 3 && args[2] == "extract") {
    auto input = GetOptionValue(args, "--input");
    auto out_dir = GetOptionValue(args, "--out-dir");
    if (!input.has_value() || !out_dir.has_value()) {
      err << "attachments extract requires --input and --out-dir\n";
      return 1;
    }

    pdf::ExtractAttachmentsRequest request;
    request.input_pdf = *input;
    request.output_dir = *out_dir;
    request.best_effort = !HasFlag(args, "--strict");

    pdf::ExtractAttachmentsResult result;
    Status status = pdf::ExtractAttachments(request, &result);
    if (!status.ok()) {
      return PrintStatus(err, status);
    }
    out << "attachments extracted count=" << result.attachments.size()
        << " parser=" << result.parser
        << " parse_failed=" << (result.parse_failed ? "yes" : "no") << "\n";
    return 0;
  }

  if (command == "pdf" && args.size() >= 3 && args[2] == "info") {
    auto input = GetOptionValue(args, "--input");
    if (!input.has_value()) {
      err << "pdf info requires --input\n";
      return 1;
    }

    pdf::PdfInfoRequest request;
    request.input_pdf = *input;
    pdf::PdfInfoResult result;
    Status status = pdf::GetPdfInfo(request, &result);
    if (!status.ok()) {
      return PrintStatus(err, status);
    }
    out << "pdf info pages=" << result.page_count << "\n";
    return 0;
  }

  if (command == "image2pdf") {
    auto output = GetOptionValue(args, "--output");
    if (!output.has_value()) {
      err << "image2pdf requires --output\n";
      return 1;
    }

    size_t images_index = 0;
    for (size_t i = 0; i < args.size(); ++i) {
      if (args[i] == "--images") {
        images_index = i;
        break;
      }
    }
    if (images_index == 0 || images_index + 1 >= args.size()) {
      err << "image2pdf requires --images <img1> <img2> ...\n";
      return 1;
    }

    pdf::ImagesToPdfRequest request;
    request.output_pdf = *output;
    request.image_paths.assign(args.begin() + static_cast<std::ptrdiff_t>(images_index + 1), args.end());
    request.use_image_size_as_page = HasFlag(args, "--use-image-size");
    request.scale_mode = HasFlag(args, "--original-size") ? pdf::ImageScaleMode::kOriginal : pdf::ImageScaleMode::kFit;

    pdf::ImagesToPdfResult result;
    Status status = pdf::ImagesToPdf(request, &result);
    if (!status.ok()) {
      return PrintStatus(err, status);
    }
    out << "image2pdf pages=" << result.page_count << " skipped_images=" << result.skipped_image_count << "\n";
    return 0;
  }

  if (command == "page" && args.size() >= 3) {
    const std::string action = args[2];
    if (action == "delete") {
      auto input = GetOptionValue(args, "--input");
      auto output = GetOptionValue(args, "--output");
      auto page = GetOptionValue(args, "--page");
      if (!input.has_value() || !output.has_value() || !page.has_value()) {
        err << "page delete requires --input --output --page\n";
        return 1;
      }

      uint32_t page_num = 0;
      if (!ParseUint32(*page, &page_num)) {
        err << "invalid --page value\n";
        return 1;
      }

      pdf::DeletePageRequest request;
      request.input_pdf = *input;
      request.output_pdf = *output;
      request.page_number = page_num;

      pdf::DeletePageResult result;
      Status status = pdf::DeletePage(request, &result);
      if (!status.ok()) {
        return PrintStatus(err, status);
      }
      out << "page delete pages=" << result.output_page_count << "\n";
      return 0;
    }

    if (action == "insert") {
      auto input = GetOptionValue(args, "--input");
      auto output = GetOptionValue(args, "--output");
      auto at = GetOptionValue(args, "--at");
      auto source = GetOptionValue(args, "--source");
      auto source_page = GetOptionValue(args, "--source-page");
      if (!input.has_value() || !output.has_value() || !at.has_value() || !source.has_value() ||
          !source_page.has_value()) {
        err << "page insert requires --input --output --at --source --source-page\n";
        return 1;
      }

      uint32_t at_num = 0;
      uint32_t source_page_num = 0;
      if (!ParseUint32(*at, &at_num) || !ParseUint32(*source_page, &source_page_num)) {
        err << "invalid numeric value for --at or --source-page\n";
        return 1;
      }

      pdf::InsertPageRequest request;
      request.input_pdf = *input;
      request.output_pdf = *output;
      request.insert_at = at_num;
      request.source_pdf = *source;
      request.source_page_number = source_page_num;

      pdf::InsertPageResult result;
      Status status = pdf::InsertPage(request, &result);
      if (!status.ok()) {
        return PrintStatus(err, status);
      }
      out << "page insert pages=" << result.output_page_count << "\n";
      return 0;
    }

    if (action == "replace") {
      auto input = GetOptionValue(args, "--input");
      auto output = GetOptionValue(args, "--output");
      auto page = GetOptionValue(args, "--page");
      auto source = GetOptionValue(args, "--source");
      auto source_page = GetOptionValue(args, "--source-page");
      if (!input.has_value() || !output.has_value() || !page.has_value() || !source.has_value() ||
          !source_page.has_value()) {
        err << "page replace requires --input --output --page --source --source-page\n";
        return 1;
      }

      uint32_t page_num = 0;
      uint32_t source_page_num = 0;
      if (!ParseUint32(*page, &page_num) || !ParseUint32(*source_page, &source_page_num)) {
        err << "invalid numeric value for --page or --source-page\n";
        return 1;
      }

      pdf::ReplacePageRequest request;
      request.input_pdf = *input;
      request.output_pdf = *output;
      request.page_number = page_num;
      request.source_pdf = *source;
      request.source_page_number = source_page_num;

      pdf::ReplacePageResult result;
      Status status = pdf::ReplacePage(request, &result);
      if (!status.ok()) {
        return PrintStatus(err, status);
      }
      out << "page replace pages=" << result.output_page_count << "\n";
      return 0;
    }
  }

  if (command == "convert" && args.size() >= 3 && args[2] == "pdf2docx") {
    auto input = GetOptionValue(args, "--input");
    auto output = GetOptionValue(args, "--output");
    if (!input.has_value() || !output.has_value()) {
      err << "convert pdf2docx requires --input and --output\n";
      return 1;
    }

    convert::PdfToDocxRequest request;
    request.input_pdf = *input;
    request.output_docx = *output;
    request.dump_ir_json_path = GetOptionValue(args, "--dump-ir").value_or("");
    request.extract_images = !HasFlag(args, "--no-images");
    request.enable_font_fallback = !HasFlag(args, "--disable-font-fallback");
    request.use_anchored_images = HasFlag(args, "--docx-anchored");

    convert::PdfToDocxResult result;
    Status status = convert::ConvertPdfToDocx(request, &result);
    if (!status.ok()) {
      return PrintStatus(err, status);
    }
    out << "pdf2docx pages=" << result.page_count << " images=" << result.image_count
        << " warnings=" << result.warning_count << "\n";
    return 0;
  }

  err << "Unknown command\n";
  PrintHelp(err);
  return 1;
}

}  // namespace pdftools
