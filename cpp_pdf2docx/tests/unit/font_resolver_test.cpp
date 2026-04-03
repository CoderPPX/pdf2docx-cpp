#include <cstdlib>

#include "pdf2docx/font_resolver.hpp"

int main() {
  pdf2docx::FontResolver resolver;
  resolver.SetAlias("Helvetica", "Arial");
  resolver.SetFallbackFamilies({"Noto Sans CJK SC", "Arial"});
  resolver.AddFontPath("Arial", "/fonts/arial.ttf");

  {
    const pdf2docx::FontResolveResult result =
        resolver.Resolve(pdf2docx::FontResolveRequest{.family = "Helvetica"});
    if (!result.found) {
      return EXIT_FAILURE;
    }
    if (result.resolved_family != "Arial") {
      return EXIT_FAILURE;
    }
    if (result.file_path != "/fonts/arial.ttf") {
      return EXIT_FAILURE;
    }
    if (result.fallback_used) {
      return EXIT_FAILURE;
    }
  }

  {
    const pdf2docx::FontResolveResult result =
        resolver.Resolve(pdf2docx::FontResolveRequest{.family = ""});
    if (!result.found) {
      return EXIT_FAILURE;
    }
    if (result.resolved_family != "Noto Sans CJK SC") {
      return EXIT_FAILURE;
    }
    if (!result.fallback_used) {
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}
