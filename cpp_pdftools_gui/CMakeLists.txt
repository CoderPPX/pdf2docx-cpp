cmake_minimum_required(VERSION 3.24)

project(pdftools_gui VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

option(PDFTOOLS_GUI_ENABLE_TESTS "Enable pdftools GUI tests" ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Concurrent Test)

set(PDFTOOLS_ENABLE_TESTS OFF CACHE BOOL "Disable pdftools core tests when built from GUI" FORCE)
set(PDFTOOLS_ENABLE_CLI OFF CACHE BOOL "Disable pdftools core CLI when built from GUI" FORCE)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../cpp_pdftools" "${CMAKE_CURRENT_BINARY_DIR}/cpp_pdftools")

set(PDFTOOLS_GUI_HEADERS
  include/pdftools_gui/app/main_window.hpp
  include/pdftools_gui/models/task_item_model.hpp
  include/pdftools_gui/pages/attachments_page.hpp
  include/pdftools_gui/pages/image_to_pdf_page.hpp
  include/pdftools_gui/pages/merge_page.hpp
  include/pdftools_gui/pages/page_edit_page.hpp
  include/pdftools_gui/pages/pdf2docx_page.hpp
  include/pdftools_gui/pages/settings_page.hpp
  include/pdftools_gui/pages/text_extract_page.hpp
  include/pdftools_gui/services/pdftools_service.hpp
  include/pdftools_gui/services/preview_service.hpp
  include/pdftools_gui/services/settings_service.hpp
  include/pdftools_gui/services/task_manager.hpp
  include/pdftools_gui/services/validation_service.hpp
  include/pdftools_gui/widgets/file_list_editor.hpp
  include/pdftools_gui/widgets/path_picker.hpp
  include/pdftools_gui/widgets/task_center_widget.hpp
)

set(PDFTOOLS_GUI_SOURCES
  src/app/main_window.cpp
  src/models/task_item_model.cpp
  src/pages/attachments_page.cpp
  src/pages/image_to_pdf_page.cpp
  src/pages/merge_page.cpp
  src/pages/page_edit_page.cpp
  src/pages/pdf2docx_page.cpp
  src/pages/settings_page.cpp
  src/pages/text_extract_page.cpp
  src/services/pdftools_service.cpp
  src/services/preview_service.cpp
  src/services/settings_service.cpp
  src/services/task_manager.cpp
  src/services/validation_service.cpp
  src/widgets/file_list_editor.cpp
  src/widgets/path_picker.cpp
  src/widgets/task_center_widget.cpp
)

add_library(pdftools_gui_lib STATIC ${PDFTOOLS_GUI_HEADERS} ${PDFTOOLS_GUI_SOURCES})

target_include_directories(pdftools_gui_lib
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(pdftools_gui_lib
  PUBLIC
    Qt6::Widgets
    Qt6::Concurrent
    pdftools::core
)

add_executable(pdftools_gui src/app/main.cpp)
target_link_libraries(pdftools_gui PRIVATE pdftools_gui_lib)

if(PDFTOOLS_GUI_ENABLE_TESTS)
  include(CTest)
  if(BUILD_TESTING)
    set(PDFTOOLS_GUI_TEST_PDF "${CMAKE_CURRENT_SOURCE_DIR}/../cpp_pdf2docx/build/test-image-text.pdf")
    if(NOT EXISTS "${PDFTOOLS_GUI_TEST_PDF}")
      set(PDFTOOLS_GUI_TEST_PDF "${CMAKE_CURRENT_SOURCE_DIR}/../cpp_pdf2docx/build/image-text.pdf")
    endif()
    if(NOT EXISTS "${PDFTOOLS_GUI_TEST_PDF}")
      set(PDFTOOLS_GUI_TEST_PDF "${CMAKE_CURRENT_SOURCE_DIR}/../cpp_pdf2docx/build/text.pdf")
    endif()

    set(PDFTOOLS_GUI_TEST_ALT_PDF "${CMAKE_CURRENT_SOURCE_DIR}/../cpp_pdf2docx/build/image.pdf")
    if(NOT EXISTS "${PDFTOOLS_GUI_TEST_ALT_PDF}")
      set(PDFTOOLS_GUI_TEST_ALT_PDF "${PDFTOOLS_GUI_TEST_PDF}")
    endif()

    set(PDFTOOLS_GUI_TEST_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/tinyxml2/TinyXML2_small.png")
    if(NOT EXISTS "${PDFTOOLS_GUI_TEST_IMAGE}")
      set(PDFTOOLS_GUI_TEST_IMAGE "")
    endif()

    message(STATUS "[gui-tests] fixture pdf: ${PDFTOOLS_GUI_TEST_PDF}")
    message(STATUS "[gui-tests] fixture alt pdf: ${PDFTOOLS_GUI_TEST_ALT_PDF}")
    message(STATUS "[gui-tests] fixture image: ${PDFTOOLS_GUI_TEST_IMAGE}")

    add_executable(pdftools_gui_service_test tests/pdftools_service_test.cpp)
    target_link_libraries(pdftools_gui_service_test PRIVATE pdftools_gui_lib Qt6::Test)
    target_compile_definitions(pdftools_gui_service_test PRIVATE
      PDFTOOLS_GUI_TEST_PDF_PATH="${PDFTOOLS_GUI_TEST_PDF}"
      PDFTOOLS_GUI_TEST_ALT_PDF_PATH="${PDFTOOLS_GUI_TEST_ALT_PDF}"
      PDFTOOLS_GUI_TEST_IMAGE_PATH="${PDFTOOLS_GUI_TEST_IMAGE}")
    add_test(NAME pdftools_gui_service_test COMMAND pdftools_gui_service_test)

    add_executable(pdftools_gui_task_item_model_test tests/task_item_model_test.cpp)
    target_link_libraries(pdftools_gui_task_item_model_test PRIVATE pdftools_gui_lib Qt6::Test)
    add_test(NAME pdftools_gui_task_item_model_test COMMAND pdftools_gui_task_item_model_test)

    add_executable(pdftools_gui_task_manager_test tests/task_manager_test.cpp)
    target_link_libraries(pdftools_gui_task_manager_test PRIVATE pdftools_gui_lib Qt6::Test)
    target_compile_definitions(pdftools_gui_task_manager_test PRIVATE
      PDFTOOLS_GUI_TEST_PDF_PATH="${PDFTOOLS_GUI_TEST_PDF}")
    add_test(NAME pdftools_gui_task_manager_test COMMAND pdftools_gui_task_manager_test)

    add_executable(pdftools_gui_settings_service_test tests/settings_service_test.cpp)
    target_link_libraries(pdftools_gui_settings_service_test PRIVATE pdftools_gui_lib Qt6::Test)
    add_test(NAME pdftools_gui_settings_service_test COMMAND pdftools_gui_settings_service_test)

    add_executable(pdftools_gui_preview_service_test tests/preview_service_test.cpp)
    target_link_libraries(pdftools_gui_preview_service_test PRIVATE pdftools_gui_lib Qt6::Test)
    target_compile_definitions(pdftools_gui_preview_service_test PRIVATE
      PDFTOOLS_GUI_TEST_PDF_PATH="${PDFTOOLS_GUI_TEST_PDF}")
    add_test(NAME pdftools_gui_preview_service_test COMMAND pdftools_gui_preview_service_test)

    add_executable(pdftools_gui_task_center_widget_test tests/task_center_widget_test.cpp)
    target_link_libraries(pdftools_gui_task_center_widget_test PRIVATE pdftools_gui_lib Qt6::Test)
    add_test(NAME pdftools_gui_task_center_widget_test COMMAND pdftools_gui_task_center_widget_test)

    add_executable(pdftools_gui_settings_page_test tests/settings_page_test.cpp)
    target_link_libraries(pdftools_gui_settings_page_test PRIVATE pdftools_gui_lib Qt6::Test)
    add_test(NAME pdftools_gui_settings_page_test COMMAND pdftools_gui_settings_page_test)

    add_executable(pdftools_gui_page_edit_page_test tests/page_edit_page_test.cpp)
    target_link_libraries(pdftools_gui_page_edit_page_test PRIVATE pdftools_gui_lib Qt6::Test)
    target_compile_definitions(pdftools_gui_page_edit_page_test PRIVATE
      PDFTOOLS_GUI_TEST_PDF_PATH="${PDFTOOLS_GUI_TEST_PDF}")
    add_test(NAME pdftools_gui_page_edit_page_test COMMAND pdftools_gui_page_edit_page_test)

    add_executable(pdftools_gui_text_extract_page_test tests/text_extract_page_test.cpp)
    target_link_libraries(pdftools_gui_text_extract_page_test PRIVATE pdftools_gui_lib Qt6::Test)
    target_compile_definitions(pdftools_gui_text_extract_page_test PRIVATE
      PDFTOOLS_GUI_TEST_PDF_PATH="${PDFTOOLS_GUI_TEST_PDF}")
    add_test(NAME pdftools_gui_text_extract_page_test COMMAND pdftools_gui_text_extract_page_test)

    add_executable(pdftools_gui_pdf2docx_page_test tests/pdf2docx_page_test.cpp)
    target_link_libraries(pdftools_gui_pdf2docx_page_test PRIVATE pdftools_gui_lib Qt6::Test)
    target_compile_definitions(pdftools_gui_pdf2docx_page_test PRIVATE
      PDFTOOLS_GUI_TEST_PDF_PATH="${PDFTOOLS_GUI_TEST_PDF}")
    add_test(NAME pdftools_gui_pdf2docx_page_test COMMAND pdftools_gui_pdf2docx_page_test)

    add_executable(pdftools_gui_image_to_pdf_page_test tests/image_to_pdf_page_test.cpp)
    target_link_libraries(pdftools_gui_image_to_pdf_page_test PRIVATE pdftools_gui_lib Qt6::Test)
    target_compile_definitions(pdftools_gui_image_to_pdf_page_test PRIVATE
      PDFTOOLS_GUI_TEST_IMAGE_PATH="${PDFTOOLS_GUI_TEST_IMAGE}")
    add_test(NAME pdftools_gui_image_to_pdf_page_test COMMAND pdftools_gui_image_to_pdf_page_test)

    add_executable(pdftools_gui_merge_page_test tests/merge_page_test.cpp)
    target_link_libraries(pdftools_gui_merge_page_test PRIVATE pdftools_gui_lib Qt6::Test)
    target_compile_definitions(pdftools_gui_merge_page_test PRIVATE
      PDFTOOLS_GUI_TEST_PDF_PATH="${PDFTOOLS_GUI_TEST_PDF}"
      PDFTOOLS_GUI_TEST_ALT_PDF_PATH="${PDFTOOLS_GUI_TEST_ALT_PDF}")
    add_test(NAME pdftools_gui_merge_page_test COMMAND pdftools_gui_merge_page_test)

    set(_PDFTOOLS_GUI_TEST_ENV "QT_QPA_PLATFORM=offscreen")
    set_tests_properties(
      pdftools_gui_service_test
      pdftools_gui_task_item_model_test
      pdftools_gui_task_manager_test
      pdftools_gui_settings_service_test
      pdftools_gui_task_center_widget_test
      pdftools_gui_settings_page_test
      pdftools_gui_page_edit_page_test
      pdftools_gui_merge_page_test
      pdftools_gui_preview_service_test
      pdftools_gui_text_extract_page_test
      pdftools_gui_pdf2docx_page_test
      pdftools_gui_image_to_pdf_page_test
      PROPERTIES ENVIRONMENT "${_PDFTOOLS_GUI_TEST_ENV}")
  endif()
endif()
