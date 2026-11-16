find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(dicom_core_tests
  # List test .cc files here as you create them, e.g.:
  # dicom_loader_test.cc
)

target_link_libraries(dicom_core_tests PRIVATE
  drtis::dicom_core
  GTest::gtest_main
)

target_compile_options(dicom_core_tests PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic -Wshadow -Wconversion>
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

gtest_discover_tests(dicom_core_tests)
