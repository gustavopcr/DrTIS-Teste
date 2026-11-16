find_package(DCMTK CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

set(DICOM_TEST_SOURCES
  dicom_image_test.cpp
  dicom_test_fixtures.cpp
)

if(DICOM_TEST_SOURCES)
  add_executable(dicom_core_tests ${DICOM_TEST_SOURCES})

  target_link_libraries(dicom_core_tests PRIVATE
    drtis::dicom_core
    GTest::gtest_main
    DCMTK::DCMTK
  )

  target_compile_options(dicom_core_tests PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic -Wshadow -Wconversion>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
  )

  gtest_discover_tests(dicom_core_tests)
else()
  message(STATUS "No test sources listed yet; skipping dicom_core_tests target.")
endif()
