cmake_minimum_required(VERSION 3.28...3.30)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)
project(${_name} VERSION ${_version} LANGUAGES C CXX)

include(compilerconfig)
include(defaults)
include(helpers)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Stream Stats 1.0 targets Windows x64")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)
find_package(Qt6 6.11.1 EXACT REQUIRED COMPONENTS Core Gui Widgets)
configure_file(src/version.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/generated/version.hpp" @ONLY)

add_library(${CMAKE_PROJECT_NAME} MODULE
  src/plugin.cpp src/stats_dock.cpp src/stats_collector.cpp
  src/stats_dock.hpp src/stats_collector.hpp src/stats_model.hpp)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
  OBS::libobs OBS::obs-frontend-api Qt6::Core Qt6::Gui Qt6::Widgets psapi pdh)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
  QT_NO_KEYWORDS WIN32_LEAN_AND_MEAN NOMINMAX)
if(MSVC)
  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE /W4 /utf-8 /permissive-)
endif()
set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

enable_testing()
add_executable(stream-stats-model-tests tests/test_model.cpp)
target_include_directories(stream-stats-model-tests PRIVATE src)
target_link_libraries(stream-stats-model-tests PRIVATE Qt6::Core)
add_test(NAME stream-aggregation-and-history COMMAND stream-stats-model-tests)
set_tests_properties(stream-aggregation-and-history PROPERTIES
  ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")

add_executable(stream-stats-ui-preview tests/ui_preview.cpp src/stats_dock.cpp src/stats_collector.cpp)
target_include_directories(stream-stats-ui-preview PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_definitions(stream-stats-ui-preview PRIVATE STREAM_STATS_PREVIEW QT_NO_KEYWORDS)
target_link_libraries(stream-stats-ui-preview PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
if(MSVC)
  target_compile_options(stream-stats-ui-preview PRIVATE /W4 /utf-8 /permissive-)
endif()
