find_package(DCMTK CONFIG REQUIRED)

add_library(dicom_core STATIC
  src/dicom/dicom_image.cpp
)
add_library(drtis::dicom_core ALIAS dicom_core)

target_compile_features(dicom_core PUBLIC cxx_std_17)

target_include_directories(dicom_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Keep DCMTK strictly PRIVATE so consumers (tests, Qt UI) cannot depend on it.
target_link_libraries(dicom_core PRIVATE DCMTK::DCMTK)

target_compile_options(dicom_core PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic -Wshadow -Wconversion>
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
