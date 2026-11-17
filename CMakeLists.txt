cmake_minimum_required(VERSION 3.21...4.5)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
  set(_drtis_vcpkg_toolchain "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
  if(EXISTS "${_drtis_vcpkg_toolchain}")
    set(CMAKE_TOOLCHAIN_FILE "${_drtis_vcpkg_toolchain}" CACHE STRING "")
  endif()
endif()

project(drtis_dicom_viewer
  VERSION 0.1.0
  DESCRIPTION "DICOM image viewer in C++17 with DCMTK and Qt"
  LANGUAGES CXX
)

get_property(_drtis_is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT _drtis_is_multi_config AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Enforce C++17 strictly across all targets without compiler-specific extensions.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generate compile_commands.json for IDE / clangd tooling.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_GUI "Build the Qt6 GUI application" ON)
option(BUILD_TESTS "Build unit tests" ON)

add_subdirectory(core)

if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(BUILD_GUI)
  add_subdirectory(app)
endif()
