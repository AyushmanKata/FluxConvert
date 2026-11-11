cmake_minimum_required(VERSION 3.20)
project(FluxConvert VERSION 1.0.1 LANGUAGES CXX)

include(GNUInstallDirs)

# C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_GUI "Build GUI application" ON)
option(BUILD_CLI "Build CLI test runner" ON)
option(ENABLE_SANITIZERS "Enable AddressSanitizer and UBSan" OFF)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(
    -Wall -Wextra -Wpedantic
    -Wconversion -Wsign-conversion
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Wcast-align
    -Wunused
    -Woverloaded-virtual
    -Wshadow
  )

  # Release optimizations
  if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    add_compile_options(-O3 -ffunction-sections -fdata-sections)
    add_link_options(-Wl,--gc-sections)
  endif()
endif()

# Sanitizers (development builds)
if(ENABLE_SANITIZERS)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
  endif()
endif()

# Find dependencies
find_package(PkgConfig REQUIRED)
find_package(ZLIB REQUIRED)

pkg_check_modules(PNG REQUIRED IMPORTED_TARGET libpng)
pkg_check_modules(JPEG REQUIRED IMPORTED_TARGET libjpeg)

# MuPDF (requires manual configuration as it often lacks pkg-config)
set(MUPDF_MIN_VERSION "1.28.6")
find_path(MUPDF_INCLUDE_DIR mupdf/fitz.h)
find_library(MUPDF_LIBRARY NAMES mupdf)
find_library(MUPDF_THIRD_LIBRARY NAMES mupdf-third)

if(NOT MUPDF_INCLUDE_DIR OR NOT MUPDF_LIBRARY)
  message(FATAL_ERROR
    "MuPDF not found. Build MuPDF ${MUPDF_MIN_VERSION} or newer and pass "
    "MUPDF_INCLUDE_DIR, MUPDF_LIBRARY, and MUPDF_THIRD_LIBRARY.")
endif()

file(STRINGS "${MUPDF_INCLUDE_DIR}/mupdf/fitz/version.h" MUPDF_VERSION_LINE
  REGEX "^#define FZ_VERSION ")
string(REGEX REPLACE ".*\"([0-9]+\\.[0-9]+\\.[0-9]+)\".*" "\\1"
  MUPDF_VERSION_MATCH
  "${MUPDF_VERSION_LINE}")
if(NOT MUPDF_VERSION_MATCH OR MUPDF_VERSION_MATCH VERSION_LESS MUPDF_MIN_VERSION)
  message(FATAL_ERROR
    "MuPDF ${MUPDF_MIN_VERSION} or newer is required; found "
    "${MUPDF_VERSION_MATCH}. Build a newer MuPDF from source and pass "
    "MUPDF_INCLUDE_DIR, MUPDF_LIBRARY, and MUPDF_THIRD_LIBRARY.")
endif()

# MuPDF transitive dependencies for static linking
find_library(MUPDF_MUJS NAMES mujs libmujs.so.3)
find_library(MUPDF_GUMBO NAMES gumbo libgumbo.so.2)
find_library(MUPDF_OPENJP2 NAMES openjp2 libopenjp2.so.7)
find_library(MUPDF_JBIG2DEC NAMES jbig2dec libjbig2dec.so.0)
find_library(MUPDF_HARFBUZZ NAMES harfbuzz)
find_library(MUPDF_FREETYPE NAMES freetype)
find_library(MUPDF_BZIP2 NAMES bz2)
find_library(MUPDF_BROTLIDEC NAMES brotlidec)
find_library(MUPDF_BROTLICOMMON NAMES brotlicommon)

set(MUPDF_ALL_LIBS ${MUPDF_LIBRARY})
if(MUPDF_THIRD_LIBRARY)
  list(APPEND MUPDF_ALL_LIBS ${MUPDF_THIRD_LIBRARY})
endif()
foreach(LIB ${MUPDF_MUJS} ${MUPDF_GUMBO} ${MUPDF_OPENJP2} ${MUPDF_JBIG2DEC}
            ${MUPDF_HARFBUZZ} ${MUPDF_FREETYPE} ${MUPDF_BZIP2}
            ${MUPDF_BROTLIDEC} ${MUPDF_BROTLICOMMON})
  if(LIB)
    list(APPEND MUPDF_ALL_LIBS ${LIB})
  endif()
endforeach()
list(APPEND MUPDF_ALL_LIBS m)

if(BUILD_GUI)
  pkg_check_modules(GTKMM REQUIRED IMPORTED_TARGET gtkmm-4.0)
endif()

# Core library
add_library(fluxconvert-core STATIC
  src/core/format.cpp
  src/core/registry.cpp
  src/core/manager.cpp
  src/core/image_data.cpp
  src/core/format_handler.cpp
  src/core/conversion_request.cpp
  src/core/conversion_result.cpp

  src/formats/png/png_handler.cpp
  src/formats/png/png_adapter.cpp

  src/formats/jpeg/jpeg_handler.cpp
  src/formats/jpeg/jpeg_adapter.cpp

  src/formats/pdf/pdf_handler.cpp
  src/formats/pdf/pdf_writer.cpp
  src/formats/pdf/pdf_renderer.cpp

  src/pipeline/job_executor.cpp
  src/pipeline/resource_limits.cpp

  src/platform/file_utils.cpp
  src/platform/tempfile.cpp
)

target_include_directories(fluxconvert-core
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(fluxconvert-core
  PUBLIC
    PkgConfig::PNG
    PkgConfig::JPEG
    ZLIB::ZLIB
    ${MUPDF_ALL_LIBS}
)

target_include_directories(fluxconvert-core PRIVATE ${MUPDF_INCLUDE_DIR})

# CLI test runner
if(BUILD_CLI)
  add_executable(fluxconvert-cli src/cli/main_cli.cpp)
  target_link_libraries(fluxconvert-cli PRIVATE fluxconvert-core)

  install(TARGETS fluxconvert-cli
    RUNTIME DESTINATION bin
  )
endif()

# GUI application
if(BUILD_GUI)
  add_executable(fluxconvert-gui
    src/gui/main.cpp
    src/gui/main_window.cpp
    src/gui/conversion_queue_view.cpp
  )

  target_link_libraries(fluxconvert-gui
    PRIVATE
      fluxconvert-core
      PkgConfig::GTKMM
  )

  install(TARGETS fluxconvert-gui
    RUNTIME DESTINATION bin
  )
endif()

# Benchmark suite
add_executable(benchmark benchmarks/main_benchmark.cpp)
target_link_libraries(benchmark PRIVATE fluxconvert-core)

# Tests
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# Installation
install(TARGETS fluxconvert-core
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY include/converter DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY docs/ DESTINATION share/doc/fluxconvert)
install(FILES LICENSE LICENSES.md THIRD_PARTY_NOTICES.md README.md
  DESTINATION share/doc/fluxconvert
)
install(FILES RELEASE_NOTES.md
  DESTINATION share/doc/fluxconvert
)

install(FILES packaging/org.fluxconvert.FluxConvert.desktop
  DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)
install(FILES packaging/org.fluxconvert.FluxConvert.svg
  DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
)
install(FILES packaging/org.fluxconvert.FluxConvert.metainfo.xml
  DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo
)

set(CPACK_PACKAGE_NAME "fluxconvert")
set(CPACK_PACKAGE_VENDOR "FluxConvert contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Offline image and PDF converter")
set(CPACK_PACKAGE_DESCRIPTION
  "FluxConvert is a free, offline desktop application for converting PNG, JPEG, and PDF files."
)
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_CONTACT "FluxConvert contributors")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "FluxConvert contributors")
set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/AyushmanKata/FluxConvert")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
include(CPack)
