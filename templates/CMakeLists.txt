# Build
cmake_minimum_required(VERSION 3.12)
project(templates VERSION 1.0.0 LANGUAGES CXX)

add_library(${PROJECT_NAME} INTERFACE)
add_library(pc::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME}
# A PUBLIC Header can be accessed outside
INTERFACE
$<INSTALL_INTERFACE:include>
$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
# A Private Header File cannot be accessed by
# Any packages that uses it
)