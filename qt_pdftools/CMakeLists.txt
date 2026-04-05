cmake_minimum_required(VERSION 3.24)

project(qt_pdftools VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Concurrent)

set(QT_PDFTOOLS_HAS_QTPDF OFF)
find_package(Qt6 6.4 QUIET COMPONENTS Pdf PdfWidgets)
if(TARGET Qt6::Pdf AND TARGET Qt6::PdfWidgets)
  set(QT_PDFTOOLS_HAS_QTPDF ON)
endif()

set(PDFTOOLS_ENABLE_TESTS OFF CACHE BOOL "Disable pdftools tests from qt_pdftools" FORCE)
set(PDFTOOLS_ENABLE_CLI OFF CACHE BOOL "Disable pdftools CLI from qt_pdftools" FORCE)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../cpp_pdftools" "${CMAKE_CURRENT_BINARY_DIR}/cpp_pdftools")

set(QT_PDFTOOLS_HEADERS
  include/qt_pdftools/app/main_window.hpp
  include/qt_pdftools/backend/backend_registry.hpp
  include/qt_pdftools/backend/native_lib_backend.hpp
  include/qt_pdftools/backend/task_backend.hpp
  include/qt_pdftools/core/app_settings.hpp
  include/qt_pdftools/core/task_types.hpp
  include/qt_pdftools/ui/pdf_tab_view.hpp
  include/qt_pdftools/ui/settings_dialog.hpp
)

set(QT_PDFTOOLS_SOURCES
  src/app/main_window.cpp
  src/backend/backend_registry.cpp
  src/backend/native_lib_backend.cpp
  src/core/app_settings.cpp
  src/ui/pdf_tab_view.cpp
  src/ui/settings_dialog.cpp
)

add_library(qt_pdftools_lib STATIC ${QT_PDFTOOLS_HEADERS} ${QT_PDFTOOLS_SOURCES})
target_include_directories(qt_pdftools_lib PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_link_libraries(qt_pdftools_lib
  PUBLIC
    Qt6::Widgets
    Qt6::Concurrent
    pdftools::core
)

if(QT_PDFTOOLS_HAS_QTPDF)
  target_compile_definitions(qt_pdftools_lib PUBLIC QT_PDFTOOLS_HAS_QTPDF=1)
  target_link_libraries(qt_pdftools_lib PUBLIC Qt6::Pdf Qt6::PdfWidgets)
endif()

add_executable(qt_pdftools src/app/main.cpp)
target_link_libraries(qt_pdftools PRIVATE qt_pdftools_lib)
