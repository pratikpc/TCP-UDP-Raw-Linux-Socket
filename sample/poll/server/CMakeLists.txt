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
pc::network
pc::pqpp
pc::lexical_cast
pc::protocol
pc::memory
)

target_compile_options(${PROJECT_NAME}
 PRIVATE
  -Wall
  -Wextra
  -Wpedantic
)
target_compile_definitions(${PROJECT_NAME}
 PRIVATE
  PC_PROFILE
  PC_NETWORK_MOCK
  PC_USE_SPINLOCKS
  PC_DISABLE_DATABASE_SUPPORT
  PC_OPTIMIZE_READ_MULTIPLE_SINGLE_SHOT
  PC_SWITCH_CLIENTS_BETWEEN_THREADS_BALANCE
)