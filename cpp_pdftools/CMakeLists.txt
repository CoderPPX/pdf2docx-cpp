cmake_minimum_required(VERSION 3.24)

project(pdftools_cpp VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PDFTOOLS_USE_BUNDLED_DEPS "Use bundled thirdparty dependencies" ON)
option(PDFTOOLS_ADD_SUBDIR_DEPS "Try add_subdirectory on bundled dependencies" ON)
option(PDFTOOLS_STRICT_DEPS "Fail configure when required dependencies are missing" OFF)
option(PDFTOOLS_ENABLE_TESTS "Enable tests" ON)
option(PDFTOOLS_ENABLE_CLI "Enable CLI tool" ON)

set(PDFTOOLS_THIRDPARTY_ROOT "${CMAKE_SOURCE_DIR}/../thirdparty" CACHE PATH "Path to third-party dependencies")
set(PDF2DOCX_XML_BACKEND "tinyxml2" CACHE STRING "tinyxml2|libxml2")
set(PDF2DOCX_PDF_BACKEND "podofo" CACHE STRING "podofo|poppler")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(Dependencies)
find_package(Threads REQUIRED)

pdf2docx_import_dependencies(
  "${PDFTOOLS_THIRDPARTY_ROOT}"
  ${PDFTOOLS_USE_BUNDLED_DEPS}
  ${PDFTOOLS_ADD_SUBDIR_DEPS}
  ${PDFTOOLS_STRICT_DEPS}
)

set(PDFTOOLS_LEGACY_SOURCES
  src/legacy_pdf2docx/core/status.cpp
  src/legacy_pdf2docx/core/ir_html.cpp
  src/legacy_pdf2docx/core/ir_json.cpp
  src/legacy_pdf2docx/api/converter.cpp
  src/legacy_pdf2docx/backend/podofo/podofo_backend.cpp
  src/legacy_pdf2docx/font/freetype_probe.cpp
  src/legacy_pdf2docx/font/font_resolver.cpp
  src/legacy_pdf2docx/pipeline/pipeline.cpp
  src/legacy_pdf2docx/docx/p0_writer.cpp
)

add_library(pdftools_pdf2docx_legacy STATIC ${PDFTOOLS_LEGACY_SOURCES})
add_library(pdftools::pdf2docx_legacy ALIAS pdftools_pdf2docx_legacy)
target_include_directories(pdftools_pdf2docx_legacy
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src/legacy_pdf2docx
)
target_compile_definitions(pdftools_pdf2docx_legacy
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
  target_link_libraries(pdftools_pdf2docx_legacy PRIVATE ${PDF2DOCX_ZLIB_TARGET})
endif()
if(PDF2DOCX_TINYXML2_TARGET)
  target_link_libraries(pdftools_pdf2docx_legacy PRIVATE ${PDF2DOCX_TINYXML2_TARGET})
endif()
if(PDF2DOCX_MINIZIP_TARGET)
  target_link_libraries(pdftools_pdf2docx_legacy PRIVATE ${PDF2DOCX_MINIZIP_TARGET})
endif()
if(PDF2DOCX_FREETYPE_TARGET)
  target_link_libraries(pdftools_pdf2docx_legacy PRIVATE ${PDF2DOCX_FREETYPE_TARGET})
endif()
if(PDF2DOCX_PODOFO_TARGET)
  target_link_libraries(pdftools_pdf2docx_legacy PRIVATE ${PDF2DOCX_PODOFO_TARGET})
  if(EXISTS "${CMAKE_BINARY_DIR}/_deps/podofo/src/podofo/podofo/auxiliary")
    target_include_directories(pdftools_pdf2docx_legacy PRIVATE "${CMAKE_BINARY_DIR}/_deps/podofo/src/podofo/podofo/auxiliary")
  endif()
endif()

add_library(pdftools_core STATIC
  src/core/status.cpp
  src/runtime/command_registry.cpp
  src/runtime/task_runner.cpp
  src/pdf/document_ops.cpp
  src/pdf/extract_ops.cpp
  src/pdf/create_ops.cpp
  src/convert/pdf2docx/pdf2docx.cpp
  src/app/cli_app.cpp
)
add_library(pdftools::core ALIAS pdftools_core)
target_include_directories(pdftools_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(pdftools_core
  PRIVATE
    pdftools_pdf2docx_legacy
    Threads::Threads
)
if(PDF2DOCX_PODOFO_TARGET)
  target_link_libraries(pdftools_core PRIVATE ${PDF2DOCX_PODOFO_TARGET})
  if(EXISTS "${CMAKE_BINARY_DIR}/_deps/podofo/src/podofo/podofo/auxiliary")
    target_include_directories(pdftools_core PRIVATE "${CMAKE_BINARY_DIR}/_deps/podofo/src/podofo/podofo/auxiliary")
  endif()
endif()

if(PDFTOOLS_ENABLE_CLI)
  add_executable(pdftools tools/cli/main.cpp)
  target_link_libraries(pdftools PRIVATE pdftools::core)
endif()

if(PDFTOOLS_ENABLE_TESTS)
  include(CTest)
  if(BUILD_TESTING)
    set(PDFTOOLS_TEST_PDF "${CMAKE_SOURCE_DIR}/../cpp_pdf2docx/build/test.pdf")
    if(NOT EXISTS "${PDFTOOLS_TEST_PDF}")
      set(PDFTOOLS_TEST_PDF "${CMAKE_SOURCE_DIR}/../cpp_pdf2docx/build/text.pdf")
    endif()
    if(NOT EXISTS "${PDFTOOLS_TEST_PDF}")
      set(PDFTOOLS_TEST_PDF "${CMAKE_SOURCE_DIR}/../cpp_pdf2docx/build/image.pdf")
    endif()

    set(PDFTOOLS_TEST_ALT_PDF "${CMAKE_SOURCE_DIR}/../cpp_pdf2docx/build/image.pdf")
    if(NOT EXISTS "${PDFTOOLS_TEST_ALT_PDF}")
      set(PDFTOOLS_TEST_ALT_PDF "${PDFTOOLS_TEST_PDF}")
    endif()

    set(PDFTOOLS_TEST_IMAGE_TEXT_PDF "${CMAKE_SOURCE_DIR}/../cpp_pdf2docx/build/test-image-text.pdf")
    if(NOT EXISTS "${PDFTOOLS_TEST_IMAGE_TEXT_PDF}")
      set(PDFTOOLS_TEST_IMAGE_TEXT_PDF "${CMAKE_SOURCE_DIR}/../cpp_pdf2docx/build/image-text.pdf")
    endif()
    if(NOT EXISTS "${PDFTOOLS_TEST_IMAGE_TEXT_PDF}")
      set(PDFTOOLS_TEST_IMAGE_TEXT_PDF "${PDFTOOLS_TEST_PDF}")
    endif()

    message(STATUS "[tests] fixture pdf: ${PDFTOOLS_TEST_PDF}")
    message(STATUS "[tests] fixture alt pdf: ${PDFTOOLS_TEST_ALT_PDF}")
    message(STATUS "[tests] fixture image-text pdf: ${PDFTOOLS_TEST_IMAGE_TEXT_PDF}")

    add_executable(pdftools_m0_runtime_test tests/unit/m0_runtime_test.cpp)
    target_link_libraries(pdftools_m0_runtime_test PRIVATE pdftools::core)
    add_test(NAME pdftools_m0_runtime_test COMMAND pdftools_m0_runtime_test)

    add_executable(pdftools_m1_pdf2docx_test tests/unit/m1_pdf2docx_test.cpp)
    target_link_libraries(pdftools_m1_pdf2docx_test PRIVATE pdftools::core)
    target_compile_definitions(pdftools_m1_pdf2docx_test PRIVATE
      PDFTOOLS_TEST_IMAGE_TEXT_PDF_PATH="${PDFTOOLS_TEST_IMAGE_TEXT_PDF}")
    add_test(NAME pdftools_m1_pdf2docx_test COMMAND pdftools_m1_pdf2docx_test)

    add_executable(pdftools_m2_document_ops_test tests/unit/m2_document_ops_test.cpp)
    target_link_libraries(pdftools_m2_document_ops_test PRIVATE pdftools::core)
    if(PDF2DOCX_PODOFO_TARGET)
      target_link_libraries(pdftools_m2_document_ops_test PRIVATE ${PDF2DOCX_PODOFO_TARGET})
      if(EXISTS "${CMAKE_BINARY_DIR}/_deps/podofo/src/podofo/podofo/auxiliary")
        target_include_directories(pdftools_m2_document_ops_test PRIVATE "${CMAKE_BINARY_DIR}/_deps/podofo/src/podofo/podofo/auxiliary")
      endif()
    endif()
    target_compile_definitions(pdftools_m2_document_ops_test PRIVATE
      PDFTOOLS_TEST_PDF_PATH="${PDFTOOLS_TEST_PDF}"
      PDFTOOLS_TEST_ALT_PDF_PATH="${PDFTOOLS_TEST_ALT_PDF}")
    add_test(NAME pdftools_m2_document_ops_test COMMAND pdftools_m2_document_ops_test)

    add_executable(pdftools_m3_extract_ops_test tests/unit/m3_extract_ops_test.cpp)
    target_link_libraries(pdftools_m3_extract_ops_test PRIVATE pdftools::core)
    if(PDF2DOCX_PODOFO_TARGET)
      target_link_libraries(pdftools_m3_extract_ops_test PRIVATE ${PDF2DOCX_PODOFO_TARGET})
      if(EXISTS "${CMAKE_BINARY_DIR}/_deps/podofo/src/podofo/podofo/auxiliary")
        target_include_directories(pdftools_m3_extract_ops_test PRIVATE "${CMAKE_BINARY_DIR}/_deps/podofo/src/podofo/podofo/auxiliary")
      endif()
    endif()
    target_compile_definitions(pdftools_m3_extract_ops_test PRIVATE
      PDFTOOLS_TEST_PDF_PATH="${PDFTOOLS_TEST_PDF}")
    add_test(NAME pdftools_m3_extract_ops_test COMMAND pdftools_m3_extract_ops_test)

    add_executable(pdftools_m4_create_ops_test tests/unit/m4_create_ops_test.cpp)
    target_link_libraries(pdftools_m4_create_ops_test PRIVATE pdftools::core)
    if(PDF2DOCX_PODOFO_TARGET)
      target_link_libraries(pdftools_m4_create_ops_test PRIVATE ${PDF2DOCX_PODOFO_TARGET})
      if(EXISTS "${CMAKE_BINARY_DIR}/_deps/podofo/src/podofo/podofo/auxiliary")
        target_include_directories(pdftools_m4_create_ops_test PRIVATE "${CMAKE_BINARY_DIR}/_deps/podofo/src/podofo/podofo/auxiliary")
      endif()
    endif()
    add_test(NAME pdftools_m4_create_ops_test COMMAND pdftools_m4_create_ops_test)

    add_executable(pdftools_m5_cli_test tests/unit/m5_cli_test.cpp)
    target_link_libraries(pdftools_m5_cli_test PRIVATE pdftools::core)
    target_compile_definitions(pdftools_m5_cli_test PRIVATE
      PDFTOOLS_TEST_PDF_PATH="${PDFTOOLS_TEST_PDF}")
    add_test(NAME pdftools_m5_cli_test COMMAND pdftools_m5_cli_test)

    add_executable(pdftools_m6_task_runner_test tests/unit/m6_task_runner_test.cpp)
    target_link_libraries(pdftools_m6_task_runner_test PRIVATE pdftools::core)
    add_test(NAME pdftools_m6_task_runner_test COMMAND pdftools_m6_task_runner_test)
  endif()
endif()

install(TARGETS pdftools_core pdftools_pdf2docx_legacy
  EXPORT pdftoolsTargets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

if(PDFTOOLS_ENABLE_CLI)
  install(TARGETS pdftools RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT pdftoolsTargets
  FILE pdftoolsTargets.cmake
  NAMESPACE pdftools::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/pdftools
)
