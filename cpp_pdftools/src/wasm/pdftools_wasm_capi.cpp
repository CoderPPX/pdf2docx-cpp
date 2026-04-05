#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "pdftools/error_handling.hpp"
#include "pdftools/convert/pdf2docx.hpp"
#include "pdftools/pdf/document_ops.hpp"

namespace {

size_t SkipWhitespace(std::string_view text, size_t pos) {
  while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])) != 0) {
    ++pos;
  }
  return pos;
}

std::string EscapeJson(std::string_view value) {
  std::string escaped;
  escaped.reserve(value.size() + 8);
  for (char ch : value) {
    switch (ch) {
      case '"':
        escaped += "\\\"";
        break;
      case '\\':
        escaped += "\\\\";
        break;
      case '\b':
        escaped += "\\b";
        break;
      case '\f':
        escaped += "\\f";
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
        if (static_cast<unsigned char>(ch) < 0x20) {
          escaped += '?';
        } else {
          escaped.push_back(ch);
        }
        break;
    }
  }
  return escaped;
}

bool WriteResponseBuffer(std::string_view response_json, uint8_t** response_ptr, uint32_t* response_len) {
  const size_t size = response_json.size();
  if (size == 0 || size > 0xFFFFFFFFu) {
    return false;
  }

  auto* buffer = static_cast<uint8_t*>(std::malloc(size));
  if (buffer == nullptr) {
    return false;
  }

  std::memcpy(buffer, response_json.data(), size);
  *response_ptr = buffer;
  *response_len = static_cast<uint32_t>(size);
  return true;
}

bool FindJsonValueStart(std::string_view json, std::string_view key, size_t* value_pos) {
  if (value_pos == nullptr) {
    return false;
  }

  const std::string token = std::string("\"") + std::string(key) + "\"";
  size_t search_pos = 0;
  while (true) {
    const size_t key_pos = json.find(token, search_pos);
    if (key_pos == std::string_view::npos) {
      return false;
    }

    size_t colon_pos = SkipWhitespace(json, key_pos + token.size());
    if (colon_pos >= json.size() || json[colon_pos] != ':') {
      search_pos = key_pos + token.size();
      continue;
    }

    size_t start = SkipWhitespace(json, colon_pos + 1);
    if (start >= json.size()) {
      return false;
    }

    *value_pos = start;
    return true;
  }
}

bool ParseJsonStringAt(std::string_view json, size_t start, std::string* value, size_t* next_pos) {
  if (value == nullptr) {
    return false;
  }
  if (start >= json.size() || json[start] != '"') {
    return false;
  }

  std::string out;
  size_t pos = start + 1;
  while (pos < json.size()) {
    const char ch = json[pos++];
    if (ch == '"') {
      *value = std::move(out);
      if (next_pos != nullptr) {
        *next_pos = pos;
      }
      return true;
    }

    if (ch == '\\') {
      if (pos >= json.size()) {
        return false;
      }
      const char esc = json[pos++];
      switch (esc) {
        case '"':
          out.push_back('"');
          break;
        case '\\':
          out.push_back('\\');
          break;
        case '/':
          out.push_back('/');
          break;
        case 'b':
          out.push_back('\b');
          break;
        case 'f':
          out.push_back('\f');
          break;
        case 'n':
          out.push_back('\n');
          break;
        case 'r':
          out.push_back('\r');
          break;
        case 't':
          out.push_back('\t');
          break;
        case 'u': {
          // Minimal handling: skip 4 hex digits and keep a placeholder.
          if (pos + 4 > json.size()) {
            return false;
          }
          pos += 4;
          out.push_back('?');
          break;
        }
        default:
          out.push_back(esc);
          break;
      }
      continue;
    }

    out.push_back(ch);
  }

  return false;
}

bool ParseJsonUIntAt(std::string_view json, size_t start, uint32_t* value, size_t* next_pos) {
  if (value == nullptr || start >= json.size()) {
    return false;
  }

  uint64_t parsed = 0;
  size_t pos = start;
  if (json[pos] == '+') {
    ++pos;
  }

  size_t digit_count = 0;
  while (pos < json.size() && std::isdigit(static_cast<unsigned char>(json[pos])) != 0) {
    parsed = (parsed * 10u) + static_cast<uint64_t>(json[pos] - '0');
    if (parsed > 0xFFFFFFFFu) {
      return false;
    }
    ++pos;
    ++digit_count;
  }

  if (digit_count == 0) {
    return false;
  }

  *value = static_cast<uint32_t>(parsed);
  if (next_pos != nullptr) {
    *next_pos = pos;
  }
  return true;
}

bool ParseJsonBoolAt(std::string_view json, size_t start, bool* value, size_t* next_pos) {
  if (value == nullptr || start >= json.size()) {
    return false;
  }

  if (json.substr(start, 4) == "true") {
    *value = true;
    if (next_pos != nullptr) {
      *next_pos = start + 4;
    }
    return true;
  }

  if (json.substr(start, 5) == "false") {
    *value = false;
    if (next_pos != nullptr) {
      *next_pos = start + 5;
    }
    return true;
  }

  return false;
}

bool ParseJsonStringField(std::string_view json, std::string_view key, std::string* value) {
  size_t start = 0;
  if (!FindJsonValueStart(json, key, &start)) {
    return false;
  }
  return ParseJsonStringAt(json, start, value, nullptr);
}

bool ParseJsonUIntField(std::string_view json, std::string_view key, uint32_t* value) {
  size_t start = 0;
  if (!FindJsonValueStart(json, key, &start)) {
    return false;
  }
  return ParseJsonUIntAt(json, start, value, nullptr);
}

bool ParseJsonUIntFieldAny(std::string_view json, std::initializer_list<std::string_view> keys, uint32_t* value) {
  if (value == nullptr) {
    return false;
  }
  for (const auto key : keys) {
    if (ParseJsonUIntField(json, key, value)) {
      return true;
    }
  }
  return false;
}

bool ParseJsonBoolField(std::string_view json, std::string_view key, bool* value) {
  size_t start = 0;
  if (!FindJsonValueStart(json, key, &start)) {
    return false;
  }
  return ParseJsonBoolAt(json, start, value, nullptr);
}

bool ParseJsonStringArrayField(std::string_view json, std::string_view key, std::vector<std::string>* values) {
  if (values == nullptr) {
    return false;
  }

  size_t start = 0;
  if (!FindJsonValueStart(json, key, &start)) {
    return false;
  }
  if (start >= json.size() || json[start] != '[') {
    return false;
  }

  values->clear();
  size_t pos = start + 1;
  while (pos < json.size()) {
    pos = SkipWhitespace(json, pos);
    if (pos >= json.size()) {
      return false;
    }

    if (json[pos] == ']') {
      return true;
    }

    std::string item;
    size_t next = 0;
    if (!ParseJsonStringAt(json, pos, &item, &next)) {
      return false;
    }
    values->push_back(std::move(item));
    pos = SkipWhitespace(json, next);

    if (pos >= json.size()) {
      return false;
    }
    if (json[pos] == ',') {
      ++pos;
      continue;
    }
    if (json[pos] == ']') {
      return true;
    }
    return false;
  }

  return false;
}

std::string BuildErrorEnvelope(const pdftools::Status& status, uint32_t request_len, std::string_view operation) {
  const std::string code = pdftools::ErrorCodeToString(status.code());
  const std::string message = EscapeJson(status.message());
  const std::string context = EscapeJson(status.context());
  const std::string operation_escaped = EscapeJson(operation);
  return std::string("{\"ok\":false,\"error\":{\"code\":\"") + code + "\",\"message\":\"" + message +
         "\",\"context\":\"" + context + "\",\"details\":{\"requestBytes\":" + std::to_string(request_len) +
         ",\"operation\":\"" + operation_escaped + "\"}}}";
}

std::string BuildSuccessEnvelope(std::string_view payload_json) {
  return std::string("{\"ok\":true,\"payload\":") + std::string(payload_json) + "}";
}

std::string BuildStatusErrorResponse(pdftools::ErrorCode code,
                                     std::string message,
                                     std::string context,
                                     uint32_t request_len,
                                     std::string_view operation) {
  const pdftools::Status status = pdftools::Status::Error(code, std::move(message), std::move(context));
  return BuildErrorEnvelope(status, request_len, operation);
}

std::string BuildUnsupported(std::string_view operation, uint32_t request_len) {
  std::string message = "WASM operation dispatcher is not implemented yet";
  std::string context = "pdftools_wasm_op";
  if (!operation.empty()) {
    message = "WASM operation '" + std::string(operation) + "' is not implemented yet";
    context += ":" + std::string(operation);
  }

  return BuildStatusErrorResponse(
      pdftools::ErrorCode::kUnsupportedFeature,
      std::move(message),
      std::move(context),
      request_len,
      operation);
}

std::string HandleMerge(std::string_view request_json, uint32_t request_len) {
  std::vector<std::string> inputs;
  if (!ParseJsonStringArrayField(request_json, "inputPdfs", &inputs) || inputs.size() < 2) {
    return BuildStatusErrorResponse(
        pdftools::ErrorCode::kInvalidArgument,
        "merge requires at least two inputPdfs",
        "pdftools_wasm_op:merge",
        request_len,
        "merge");
  }

  std::string output_pdf;
  if (!ParseJsonStringField(request_json, "outputPdf", &output_pdf) || output_pdf.empty()) {
    return BuildStatusErrorResponse(
        pdftools::ErrorCode::kInvalidArgument,
        "merge requires outputPdf",
        "pdftools_wasm_op:merge",
        request_len,
        "merge");
  }

  bool overwrite = false;
  (void)ParseJsonBoolField(request_json, "overwrite", &overwrite);

  pdftools::pdf::MergePdfRequest request;
  request.input_pdfs = std::move(inputs);
  request.output_pdf = output_pdf;
  request.overwrite = overwrite;

  pdftools::pdf::MergePdfResult result;
  const pdftools::Status status = pdftools::pdf::MergePdf(request, &result);
  if (!status.ok()) {
    return BuildErrorEnvelope(status, request_len, "merge");
  }

  const std::string payload = std::string("{\"operation\":\"merge\",\"outputPdf\":\"") +
                              EscapeJson(output_pdf) + "\",\"outputPageCount\":" +
                              std::to_string(result.output_page_count) + "}";
  return BuildSuccessEnvelope(payload);
}

std::string HandleDeletePage(std::string_view request_json, uint32_t request_len) {
  std::string input_pdf;
  std::string output_pdf;
  uint32_t page = 0;

  if (!ParseJsonStringField(request_json, "inputPdf", &input_pdf) || input_pdf.empty()) {
    return BuildStatusErrorResponse(
        pdftools::ErrorCode::kInvalidArgument,
        "delete-page requires inputPdf",
        "pdftools_wasm_op:delete-page",
        request_len,
        "delete-page");
  }
  if (!ParseJsonStringField(request_json, "outputPdf", &output_pdf) || output_pdf.empty()) {
    return BuildStatusErrorResponse(
        pdftools::ErrorCode::kInvalidArgument,
        "delete-page requires outputPdf",
        "pdftools_wasm_op:delete-page",
        request_len,
        "delete-page");
  }
  if (!ParseJsonUIntField(request_json, "page", &page) || page == 0) {
    return BuildStatusErrorResponse(
        pdftools::ErrorCode::kInvalidArgument,
        "delete-page requires positive page",
        "pdftools_wasm_op:delete-page",
        request_len,
        "delete-page");
  }

  bool overwrite = false;
  (void)ParseJsonBoolField(request_json, "overwrite", &overwrite);

  pdftools::pdf::DeletePageRequest request;
  request.input_pdf = input_pdf;
  request.output_pdf = output_pdf;
  request.page_number = page;
  request.overwrite = overwrite;

  pdftools::pdf::DeletePageResult result;
  const pdftools::Status status = pdftools::pdf::DeletePage(request, &result);
  if (!status.ok()) {
    return BuildErrorEnvelope(status, request_len, "delete-page");
  }

  const std::string payload = std::string("{\"operation\":\"delete-page\",\"outputPdf\":\"") +
                              EscapeJson(output_pdf) + "\",\"outputPageCount\":" +
                              std::to_string(result.output_page_count) + "}";
  return BuildSuccessEnvelope(payload);
}

std::string HandleInsertPage(std::string_view request_json, uint32_t request_len) {
  std::string input_pdf;
  std::string source_pdf;
  std::string output_pdf;
  uint32_t insert_at = 0;
  uint32_t source_page = 0;

  if (!ParseJsonStringField(request_json, "inputPdf", &input_pdf) || input_pdf.empty()) {
    return BuildStatusErrorResponse(
        pdftools::ErrorCode::kInvalidArgument,
        "insert-page requires inputPdf",
        "pdftools_wasm_op:insert-page",
        request_len,
        "insert-page");
  }
  if (!ParseJsonStringField(request_json, "sourcePdf", &source_pdf) || source_pdf.empty()) {
    return BuildStatusErrorResponse(
        pdftools::ErrorCode::kInvalidArgument,
        "insert-page requires sourcePdf",
        "pdftools_wasm_op:insert-page",
        request_len,
        "insert-page");
  }
  if (!ParseJsonStringField(request_json, "outputPdf", &output_pdf) || output_pdf.empty()) {
    return BuildStatusErrorResponse(
        pdftools::ErrorCode::kInvalidArgument,
        "insert-page requires outputPdf",
        "pdftools_wasm_op:insert-page",
        request_len,
        "insert-page");
  }
  if (!ParseJsonUIntFieldAny(request_json, {"at", "insertAt"}, &insert_at) || insert_at == 0) {
    return BuildStatusErrorResponse(
        pdftools::ErrorCode::kInvalidArgument,
        "insert-page requires positive at/insertAt",
        "pdftools_wasm_op:insert-page",
        request_len,
        "insert-page");
  }
  if (!ParseJsonUIntFieldAny(request_json, {"sourcePage", "sourcePageNumber"}, &source_page) || source_page == 0) {
    return BuildStatusErrorResponse(
        pdftools::ErrorCode::kInvalidArgument,
        "insert-page requires positive sourcePage/sourcePageNumber",
        "pdftools_wasm_op:insert-page",
        request_len,
        "insert-page");
  }

  bool overwrite = false;
  (void)ParseJsonBoolField(request_json, "overwrite", &overwrite);

  pdftools::pdf::InsertPageRequest request;
  request.input_pdf = input_pdf;
  request.output_pdf = output_pdf;
  request.insert_at = insert_at;
  request.source_pdf = source_pdf;
  request.source_page_number = source_page;
  request.overwrite = overwrite;

  pdftools::pdf::InsertPageResult result;
  const pdftools::Status status = pdftools::pdf::InsertPage(request, &result);
  if (!status.ok()) {
    return BuildErrorEnvelope(status, request_len, "insert-page");
  }

  const std::string payload = std::string("{\"operation\":\"insert-page\",\"outputPdf\":\"") +
                              EscapeJson(output_pdf) + "\",\"outputPageCount\":" +
                              std::to_string(result.output_page_count) + "}";
  return BuildSuccessEnvelope(payload);
}

std::string HandleReplacePage(std::string_view request_json, uint32_t request_len) {
  std::string input_pdf;
  std::string source_pdf;
  std::string output_pdf;
  uint32_t page = 0;
  uint32_t source_page = 0;

  if (!ParseJsonStringField(request_json, "inputPdf", &input_pdf) || input_pdf.empty()) {
    return BuildStatusErrorResponse(
        pdftools::ErrorCode::kInvalidArgument,
        "replace-page requires inputPdf",
        "pdftools_wasm_op:replace-page",
        request_len,
        "replace-page");
  }
  if (!ParseJsonStringField(request_json, "sourcePdf", &source_pdf) || source_pdf.empty()) {
    return BuildStatusErrorResponse(
        pdftools::ErrorCode::kInvalidArgument,
        "replace-page requires sourcePdf",
        "pdftools_wasm_op:replace-page",
        request_len,
        "replace-page");
  }
  if (!ParseJsonStringField(request_json, "outputPdf", &output_pdf) || output_pdf.empty()) {
    return BuildStatusErrorResponse(
        pdftools::ErrorCode::kInvalidArgument,
        "replace-page requires outputPdf",
        "pdftools_wasm_op:replace-page",
        request_len,
        "replace-page");
  }
  if (!ParseJsonUIntField(request_json, "page", &page) || page == 0) {
    return BuildStatusErrorResponse(
        pdftools::ErrorCode::kInvalidArgument,
        "replace-page requires positive page",
        "pdftools_wasm_op:replace-page",
        request_len,
        "replace-page");
  }
  if (!ParseJsonUIntFieldAny(request_json, {"sourcePage", "sourcePageNumber"}, &source_page) || source_page == 0) {
    return BuildStatusErrorResponse(
        pdftools::ErrorCode::kInvalidArgument,
        "replace-page requires positive sourcePage/sourcePageNumber",
        "pdftools_wasm_op:replace-page",
        request_len,
        "replace-page");
  }

  bool overwrite = false;
  (void)ParseJsonBoolField(request_json, "overwrite", &overwrite);

  pdftools::pdf::ReplacePageRequest request;
  request.input_pdf = input_pdf;
  request.output_pdf = output_pdf;
  request.page_number = page;
  request.source_pdf = source_pdf;
  request.source_page_number = source_page;
  request.overwrite = overwrite;

  pdftools::pdf::ReplacePageResult result;
  const pdftools::Status status = pdftools::pdf::ReplacePage(request, &result);
  if (!status.ok()) {
    return BuildErrorEnvelope(status, request_len, "replace-page");
  }

  const std::string payload = std::string("{\"operation\":\"replace-page\",\"outputPdf\":\"") +
                              EscapeJson(output_pdf) + "\",\"outputPageCount\":" +
                              std::to_string(result.output_page_count) + "}";
  return BuildSuccessEnvelope(payload);
}

std::string HandlePdf2Docx(std::string_view request_json, uint32_t request_len) {
  std::string input_pdf;
  std::string output_docx;
  std::string dump_ir_path;

  if (!ParseJsonStringField(request_json, "inputPdf", &input_pdf) || input_pdf.empty()) {
    return BuildStatusErrorResponse(
        pdftools::ErrorCode::kInvalidArgument,
        "pdf2docx requires inputPdf",
        "pdftools_wasm_op:pdf2docx",
        request_len,
        "pdf2docx");
  }
  if (!ParseJsonStringField(request_json, "outputDocx", &output_docx) || output_docx.empty()) {
    return BuildStatusErrorResponse(
        pdftools::ErrorCode::kInvalidArgument,
        "pdf2docx requires outputDocx",
        "pdftools_wasm_op:pdf2docx",
        request_len,
        "pdf2docx");
  }
  (void)ParseJsonStringField(request_json, "dumpIrPath", &dump_ir_path);
  if (dump_ir_path.empty()) {
    (void)ParseJsonStringField(request_json, "dumpIrJsonPath", &dump_ir_path);
  }

  bool no_images = false;
  bool anchored_images = false;
  bool enable_font_fallback = true;
  bool overwrite = false;
  (void)ParseJsonBoolField(request_json, "noImages", &no_images);
  (void)ParseJsonBoolField(request_json, "anchoredImages", &anchored_images);
  (void)ParseJsonBoolField(request_json, "enableFontFallback", &enable_font_fallback);
  (void)ParseJsonBoolField(request_json, "overwrite", &overwrite);

  pdftools::convert::PdfToDocxRequest request;
  request.input_pdf = input_pdf;
  request.output_docx = output_docx;
  request.dump_ir_json_path = dump_ir_path;
  request.extract_images = !no_images;
  request.enable_font_fallback = enable_font_fallback;
  request.use_anchored_images = anchored_images;
  request.overwrite = overwrite;

  pdftools::convert::PdfToDocxResult result;
  const pdftools::Status status = pdftools::convert::ConvertPdfToDocx(request, &result);
  if (!status.ok()) {
    return BuildErrorEnvelope(status, request_len, "pdf2docx");
  }

  const std::string payload =
      std::string("{\"operation\":\"pdf2docx\",\"outputDocx\":\"") + EscapeJson(output_docx) +
      "\",\"pageCount\":" + std::to_string(result.page_count) + ",\"imageCount\":" +
      std::to_string(result.image_count) + ",\"warningCount\":" + std::to_string(result.warning_count) +
      ",\"backend\":\"" + EscapeJson(result.backend) + "\",\"elapsedMs\":" + std::to_string(result.elapsed_ms) + "}";
  return BuildSuccessEnvelope(payload);
}

std::string DispatchRequest(std::string_view request_json) {
  std::string operation;
  if (!ParseJsonStringField(request_json, "type", &operation)) {
    (void)ParseJsonStringField(request_json, "command", &operation);
  }

  if (operation == "ping") {
    return BuildSuccessEnvelope("{\"pong\":true,\"mode\":\"preview\"}");
  }

  if (operation == "version") {
    return BuildSuccessEnvelope("{\"version\":\"0.1.0-wasm-preview\",\"mode\":\"preview\"}");
  }

  if (operation == "merge") {
    return HandleMerge(request_json, static_cast<uint32_t>(request_json.size()));
  }

  if (operation == "delete-page") {
    return HandleDeletePage(request_json, static_cast<uint32_t>(request_json.size()));
  }

  if (operation == "insert-page") {
    return HandleInsertPage(request_json, static_cast<uint32_t>(request_json.size()));
  }

  if (operation == "replace-page") {
    return HandleReplacePage(request_json, static_cast<uint32_t>(request_json.size()));
  }

  if (operation == "pdf2docx") {
    return HandlePdf2Docx(request_json, static_cast<uint32_t>(request_json.size()));
  }

  return BuildUnsupported(operation, static_cast<uint32_t>(request_json.size()));
}

}  // namespace

extern "C" {

int pdftools_wasm_op(const uint8_t* request_ptr,
                     uint32_t request_len,
                     uint8_t** response_ptr,
                     uint32_t* response_len) noexcept {
  if (response_ptr == nullptr || response_len == nullptr) {
    return -1;
  }

  *response_ptr = nullptr;
  *response_len = 0;

  try {
    if (request_ptr == nullptr && request_len > 0) {
      const pdftools::Status status = pdftools::Status::Error(
          pdftools::ErrorCode::kInvalidArgument,
          "request_ptr is null while request_len > 0",
          "pdftools_wasm_op");
      const std::string response = BuildErrorEnvelope(status, request_len, "invalid-request");
      if (!WriteResponseBuffer(response, response_ptr, response_len)) {
        return -2;
      }
      return 0;
    }

    const std::string_view request_json(
        reinterpret_cast<const char*>(request_ptr),
        static_cast<size_t>(request_len));
    const std::string response = DispatchRequest(request_json);
    if (!WriteResponseBuffer(response, response_ptr, response_len)) {
      return -2;
    }
    return 0;
  } catch (...) {
    const pdftools::Status status = pdftools::CurrentExceptionToStatus(
        pdftools::ErrorCode::kInternalError,
        "unknown exception in pdftools_wasm_op",
        "pdftools_wasm_op");
    const std::string response = BuildErrorEnvelope(status, request_len, "internal-exception");
    if (!WriteResponseBuffer(response, response_ptr, response_len)) {
      return -3;
    }
    return 0;
  }
}

void pdftools_wasm_free(uint8_t* ptr) noexcept {
  std::free(ptr);
}

}  // extern "C"
