cmake_minimum_required(VERSION 3.21...4.5)

project(drtis_dicom_viewer
  VERSION 0.1.0
  DESCRIPTION "DICOM image viewer in C++17 with DCMTK and Qt"
  LANGUAGES CXX
)

# Enforce C++17 strictly across all targets without compiler-specific extensions.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generate compile_commands.json for IDE / clangd tooling.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_GUI "Build the Qt6 GUI application" OFF)
option(BUILD_TESTS "Build unit tests" ON)

add_subdirectory(core)

if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(BUILD_GUI)
  add_subdirectory(app)
endif()
