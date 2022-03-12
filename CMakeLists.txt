cmake_minimum_required(VERSION 3.11)

project(basium
  VERSION 0.0.1
  DESCRIPTION "The standalone Chromium base library"
  LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/build")

option(CMAKE_EXPORT_COMPILE_COMMANDS "Exports compile_commands.json" ON)
# PAWN compiler driver always built as a shared library.
# So it is strongly recommended to enable `BUILD_SHARED_LIBS`.
option(BUILD_SHARED_LIBS "Build libraries as shared" ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

set(BASIUM_COMMON_INCLUDE_DIR
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(build)
