cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(tcp-udp-server
	VERSION 1.0.0
	DESCRIPTION
	"A basic TCP/UDP Server")

add_library(${PROJECT_NAME} INTERFACE)
add_library(pc::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

# Adding the install interface generator expression makes sure that the include
# files are installed to the proper location (provided by GNUInstallDirs)
target_include_directories(
  ${PROJECT_NAME}
  INTERFACE $<BUILD_INTERFACE:${${PROJECT_NAME}_SOURCE_DIR}/include>
			$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_98)

target_link_libraries(${PROJECT_NAME}
INTERFACE
pc::threads
pc::memory
pc::deadliner
)

