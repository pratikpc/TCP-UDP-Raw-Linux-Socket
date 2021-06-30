project(sample-tcp-poll-server)

set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_executable(${PROJECT_NAME})
file(GLOB_RECURSE LIBRARY_CXX_SOURCES src/*.cxx)
file(GLOB_RECURSE LIBRARY_HXX_SOURCES include/*.hxx)

target_sources(${PROJECT_NAME}
PRIVATE
${LIBRARY_CXX_SOURCES}
${LIBRARY_HXX_SOURCES}
)
target_compile_options(${PROJECT_NAME}
 PRIVATE
  -Wall
  -Wextra
  -Wpedantic
)

target_link_libraries(${PROJECT_NAME}
PRIVATE
pc::tcp-udp-server
pc::loadbalancer
)

target_compile_options(${PROJECT_NAME}
 PRIVATE
  -Wall
  -Wextra
  -Wpedantic
)