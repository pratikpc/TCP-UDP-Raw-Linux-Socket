cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(root)

add_subdirectory(network)
add_subdirectory(threads)
add_subdirectory(memory)
add_subdirectory(loadbalancer)
add_subdirectory(deadliner)
add_subdirectory(pqpp)
add_subdirectory(queue)
add_subdirectory(templates)
add_subdirectory(lexical_cast)
add_subdirectory(protocol)
add_subdirectory(opt)

add_subdirectory(sample)