cmake_minimum_required(VERSION 3.24)

project(pdf2docx_cpp VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PDF2DOCX_USE_BUNDLED_DEPS "Use third_party dependencies" ON)
option(PDF2DOCX_ADD_SUBDIR_DEPS "Try add_subdirectory on third_party dependencies" ON)
option(PDF2DOCX_STRICT_DEPS "Fail configure when required dependencies are missing" OFF)
option(PDF2DOCX_BUILD_SHARED "Build shared library" OFF)
option(PDF2DOCX_ENABLE_TESTS "Enable tests" ON)
option(PDF2DOCX_ENABLE_CLI "Enable CLI tool" ON)

set(PDF2DOCX_XML_BACKEND "tinyxml2" CACHE STRING "tinyxml2|libxml2")
set(PDF2DOCX_PDF_BACKEND "podofo" CACHE STRING "podofo|poppler")
set(PDF2DOCX_THIRDPARTY_ROOT "${CMAKE_SOURCE_DIR}/../thirdparty" CACHE PATH "Path to third-party dependencies")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(Dependencies)

pdf2docx_import_dependencies(
  "${PDF2DOCX_THIRDPARTY_ROOT}"
  ${PDF2DOCX_USE_BUNDLED_DEPS}
  ${PDF2DOCX_ADD_SUBDIR_DEPS}
  ${PDF2DOCX_STRICT_DEPS}
)

set(PDF2DOCX_LIBRARY_TYPE STATIC)
if(PDF2DOCX_BUILD_SHARED)
  set(PDF2DOCX_LIBRARY_TYPE SHARED)
endif()

add_library(pdf2docx_core ${PDF2DOCX_LIBRARY_TYPE}
  src/core/status.cpp
  src/core/ir_html.cpp
  src/core/ir_json.cpp
  src/api/converter.cpp
  src/backend/podofo/podofo_backend.cpp
  src/font/freetype_probe.cpp
  src/font/font_resolver.cpp
  src/pipeline/pipeline.cpp
  src/docx/p0_writer.cpp
)

add_library(pdf2docx::core ALIAS pdf2docx_core)

target_include_directories(pdf2docx_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(pdf2docx_core
  PUBLIC
    PDF2DOCX_XML_BACKEND_STR="${PDF2DOCX_XML_BACKEND}"
    PDF2DOCX_PDF_BACKEND_STR="${PDF2DOCX_PDF_BACKEND}"
    PDF2DOCX_HAS_ZLIB=${PDF2DOCX_HAS_ZLIB}
    PDF2DOCX_HAS_TINYXML2=${PDF2DOCX_HAS_TINYXML2}
    PDF2DOCX_HAS_MINIZIP=${PDF2DOCX_HAS_MINIZIP}
    PDF2DOCX_HAS_FREETYPE=${PDF2DOCX_HAS_FREETYPE}
    PDF2DOCX_HAS_PODOFO=${PDF2DOCX_HAS_PODOFO}
)

if(PDF2DOCX_ZLIB_TARGET)
  target_link_libraries(pdf2docx_core PRIVATE ${PDF2DOCX_ZLIB_TARGET})
endif()
if(PDF2DOCX_TINYXML2_TARGET)
  target_link_libraries(pdf2docx_core PRIVATE ${PDF2DOCX_TINYXML2_TARGET})
endif()
if(PDF2DOCX_MINIZIP_TARGET)
  target_link_libraries(pdf2docx_core PRIVATE ${PDF2DOCX_MINIZIP_TARGET})
endif()
if(PDF2DOCX_FREETYPE_TARGET)
  target_link_libraries(pdf2docx_core PRIVATE ${PDF2DOCX_FREETYPE_TARGET})
endif()
if(PDF2DOCX_PODOFO_TARGET)
  target_link_libraries(pdf2docx_core PRIVATE ${PDF2DOCX_PODOFO_TARGET})
  if(EXISTS "${CMAKE_BINARY_DIR}/_deps/podofo/src/podofo/podofo/auxiliary")
    target_include_directories(pdf2docx_core PRIVATE "${CMAKE_BINARY_DIR}/_deps/podofo/src/podofo/podofo/auxiliary")
  endif()
endif()

if(PDF2DOCX_ENABLE_CLI)
  add_executable(pdf2docx_cli tools/cli/main.cpp)
  target_link_libraries(pdf2docx_cli PRIVATE pdf2docx::core)
  set_target_properties(pdf2docx_cli PROPERTIES OUTPUT_NAME pdf2docx)

  add_executable(ir2html tools/ir2html/main.cpp)
  target_link_libraries(ir2html PRIVATE pdf2docx::core)

  add_executable(pdf2ir tools/pdf2ir/main.cpp)
  target_link_libraries(pdf2ir PRIVATE pdf2docx::core)
endif()

if(PDF2DOCX_ENABLE_TESTS)
  include(CTest)
  if(BUILD_TESTING)
    set(PDF2DOCX_TEST_PDF "${CMAKE_SOURCE_DIR}/build/test.pdf")
    if(NOT EXISTS "${PDF2DOCX_TEST_PDF}")
      set(PDF2DOCX_TEST_PDF "${CMAKE_SOURCE_DIR}/build/image.pdf")
    endif()
    if(NOT EXISTS "${PDF2DOCX_TEST_PDF}")
      set(PDF2DOCX_TEST_PDF "${CMAKE_SOURCE_DIR}/build/text.pdf")
    endif()

    set(PDF2DOCX_TEST_IMAGE_TEXT_PDF "${CMAKE_SOURCE_DIR}/build/test-image-text.pdf")
    if(NOT EXISTS "${PDF2DOCX_TEST_IMAGE_TEXT_PDF}")
      set(PDF2DOCX_TEST_IMAGE_TEXT_PDF "${CMAKE_SOURCE_DIR}/build/image-text.pdf")
    endif()
    if(NOT EXISTS "${PDF2DOCX_TEST_IMAGE_TEXT_PDF}")
      set(PDF2DOCX_TEST_IMAGE_TEXT_PDF "${PDF2DOCX_TEST_PDF}")
    endif()

    message(STATUS "[tests] fixture pdf: ${PDF2DOCX_TEST_PDF}")
    message(STATUS "[tests] fixture image-text pdf: ${PDF2DOCX_TEST_IMAGE_TEXT_PDF}")

    add_executable(pdf2docx_status_test tests/unit/status_test.cpp)
    target_link_libraries(pdf2docx_status_test PRIVATE pdf2docx::core)
    add_test(NAME status_test COMMAND pdf2docx_status_test)

    add_executable(pdf2docx_backend_test tests/unit/backend_test.cpp)
    target_link_libraries(pdf2docx_backend_test PRIVATE pdf2docx::core)
    target_include_directories(pdf2docx_backend_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME backend_test COMMAND pdf2docx_backend_test)

    add_executable(pdf2docx_writer_test tests/unit/writer_test.cpp)
    target_link_libraries(pdf2docx_writer_test PRIVATE pdf2docx::core)
    target_include_directories(pdf2docx_writer_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME writer_test COMMAND pdf2docx_writer_test)

    add_executable(pdf2docx_converter_test tests/unit/converter_test.cpp)
    target_link_libraries(pdf2docx_converter_test PRIVATE pdf2docx::core)
    target_compile_definitions(pdf2docx_converter_test PRIVATE
      PDF2DOCX_TEST_PDF_PATH="${PDF2DOCX_TEST_PDF}"
      PDF2DOCX_TEST_IMAGE_TEXT_PDF_PATH="${PDF2DOCX_TEST_IMAGE_TEXT_PDF}")
    add_test(NAME converter_test COMMAND pdf2docx_converter_test)

    add_executable(pdf2docx_freetype_test tests/unit/freetype_test.cpp)
    target_link_libraries(pdf2docx_freetype_test PRIVATE pdf2docx::core)
    add_test(NAME freetype_test COMMAND pdf2docx_freetype_test)

    add_executable(pdf2docx_font_resolver_test tests/unit/font_resolver_test.cpp)
    target_link_libraries(pdf2docx_font_resolver_test PRIVATE pdf2docx::core)
    add_test(NAME font_resolver_test COMMAND pdf2docx_font_resolver_test)

    add_executable(pdf2docx_pipeline_test tests/unit/pipeline_test.cpp)
    target_link_libraries(pdf2docx_pipeline_test PRIVATE pdf2docx::core)
    target_include_directories(pdf2docx_pipeline_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME pipeline_test COMMAND pdf2docx_pipeline_test)

    add_executable(pdf2docx_ir_html_test tests/unit/ir_html_test.cpp)
    target_link_libraries(pdf2docx_ir_html_test PRIVATE pdf2docx::core)
    add_test(NAME ir_html_test COMMAND pdf2docx_ir_html_test)

    add_executable(pdf2docx_ir_html_only_page_test tests/unit/ir_html_only_page_test.cpp)
    target_link_libraries(pdf2docx_ir_html_only_page_test PRIVATE pdf2docx::core)
    add_test(NAME ir_html_only_page_test COMMAND pdf2docx_ir_html_only_page_test)

    add_executable(pdf2docx_ir_html_image_test tests/unit/ir_html_image_test.cpp)
    target_link_libraries(pdf2docx_ir_html_image_test PRIVATE pdf2docx::core)
    add_test(NAME ir_html_image_test COMMAND pdf2docx_ir_html_image_test)

    add_executable(pdf2docx_ir_json_test tests/unit/ir_json_test.cpp)
    target_link_libraries(pdf2docx_ir_json_test PRIVATE pdf2docx::core)
    add_test(NAME ir_json_test COMMAND pdf2docx_ir_json_test)

    add_executable(pdf2docx_build_info_test tests/unit/build_info_test.cpp)
    target_link_libraries(pdf2docx_build_info_test PRIVATE pdf2docx::core)
    add_test(NAME build_info_test COMMAND pdf2docx_build_info_test)

    add_executable(pdf2docx_docx_image_test tests/unit/docx_image_test.cpp)
    target_link_libraries(pdf2docx_docx_image_test PRIVATE pdf2docx::core)
    target_include_directories(pdf2docx_docx_image_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME docx_image_test COMMAND pdf2docx_docx_image_test)

    add_executable(pdf2docx_docx_anchor_test tests/unit/docx_anchor_test.cpp)
    target_link_libraries(pdf2docx_docx_anchor_test PRIVATE pdf2docx::core)
    target_include_directories(pdf2docx_docx_anchor_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME docx_anchor_test COMMAND pdf2docx_docx_anchor_test)

    add_executable(pdf2docx_test_pdf_fixture_test tests/unit/test_pdf_fixture_test.cpp)
    target_link_libraries(pdf2docx_test_pdf_fixture_test PRIVATE pdf2docx::core)
    target_compile_definitions(pdf2docx_test_pdf_fixture_test PRIVATE PDF2DOCX_TEST_PDF_PATH="${PDF2DOCX_TEST_PDF}")
    add_test(NAME test_pdf_fixture_test COMMAND pdf2docx_test_pdf_fixture_test)

    add_executable(pdf2docx_integration_e2e_test tests/integration/end_to_end_test.cpp)
    target_link_libraries(pdf2docx_integration_e2e_test PRIVATE pdf2docx::core)
    target_compile_definitions(pdf2docx_integration_e2e_test PRIVATE
      PDF2DOCX_TEST_PDF_PATH="${PDF2DOCX_TEST_PDF}"
      PDF2DOCX_TEST_IMAGE_TEXT_PDF_PATH="${PDF2DOCX_TEST_IMAGE_TEXT_PDF}")
    add_test(NAME integration_e2e_test COMMAND pdf2docx_integration_e2e_test)
  endif()
endif()

install(TARGETS pdf2docx_core
  EXPORT pdf2docxTargets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

if(PDF2DOCX_ENABLE_CLI)
  install(TARGETS pdf2docx_cli ir2html pdf2ir RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT pdf2docxTargets
  FILE pdf2docxTargets.cmake
  NAMESPACE pdf2docx::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/pdf2docx
)
