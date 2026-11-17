find_package(Qt6 REQUIRED COMPONENTS Widgets)

file(GLOB_RECURSE APP_CPP_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
file(GLOB_RECURSE APP_HEADER_AND_UI_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.ui"
)

if(APP_CPP_SOURCES)
  qt_add_executable(drtis_viewer
    ${APP_CPP_SOURCES}
    ${APP_HEADER_AND_UI_SOURCES}
  )

  set_target_properties(drtis_viewer PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
    AUTOUIC_SEARCH_PATHS
      "${CMAKE_CURRENT_SOURCE_DIR};${CMAKE_CURRENT_SOURCE_DIR}/src;${CMAKE_CURRENT_SOURCE_DIR}/ui"
  )

  target_include_directories(drtis_viewer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

  target_link_libraries(drtis_viewer PRIVATE
    drtis::dicom_core
    Qt6::Widgets
  )

  target_compile_options(drtis_viewer PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic -Wshadow -Wconversion>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
  )
else()
  message(STATUS "No app .cpp sources found yet; skipping drtis_viewer target.")
endif()
