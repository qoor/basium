# Copyright 2022 MaBling <akck0918@gmail.com>. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generates a header with preprocessor defines specified by the build file.
#
# The flags are converted to function-style defines with mangled names and
# code uses an accessor macro to access the values. This is to try to
# minimize bugs where code checks whether something is defined or not, and
# the proper header isn't included, meaning the answer will always be silently
# false or might vary across the code base.
#
# In the CMake function, specify build flags in the function as a list
# of strings that encode key/value pairs like this:
#
#   buildflag_header(... FLAGS ENABLE_FOO=1 ENABLE_BAR=${enable_bar})
#
# The CMake variable values "true" and "false" will be mapped to 0 and 1 for boolean
# #if flags to be expressed naturally. This means you can't directly make a
# define that generates C++ value of true or false for use in code. If you
# REALLY need this, you can also use the string "(true)" and "(false)" to
# prevent the rewriting.

# To check the value of the flag in C code:
#
#   #include "path/to/here/header_file.h"
#
#   #if BUILDFLAG(ENABLE_FOO)
#   ...
#   #endif
#
#   const char kSpamServerUrl[] = BUILDFLAG(SPAM_SERVER_URL);
#
# There will be no #define called ENABLE_FOO so if you accidentally test for
# that in an ifdef it will always be negative.
#
#
# Template parameters
#
#   FLAGS [required, list of strings]
#       Flag values as described above.
#
#   HEADER [required, string]
#       File name for generated header. By default, this will go in the
#       generated file directory for this target, and you would include it
#       with:
#         #include "<path_to_this_BUILD_file>/<header>"
#
#   HEADER_DIR [optional, string]
#       Override the default location of the generated header. The string will
#       be treated as a subdirectory of the ${CMAKE_BINARY_DIR}. For example:
#         header_dir = "foo/bar"
#       Then you can include the header as:
#         #include "foo/bar/baz.h"
#
#   DEPENDS
#       Normal meaning.
#
#
#
#
# Example
#   buildflag_header("foo_buildflags"
#     HEADER foo_buildflags.h
#     FLAGS
#       # This uses CMake variable enable_doom_melon as the definition.
#       ENABLE_DOOM_MELON=${ENABLE_DOOM_MELON}
#
#       # This force-enables the flag.
#       ENABLE_SPACE_LASER=true
#
#       # This will expand to the quoted C string when used in source code.
#       SPAM_SERVER_URL=\"http://www.example.com/\")

cmake_minimum_required(VERSION 3.17)

function(buildflag_header TARGET_NAME)
  cmake_path(RELATIVE_PATH CMAKE_CURRENT_FUNCTION_LIST_DIR BASE_DIRECTORY ${PROJECT_BINARY_DIR} OUTPUT_VARIABLE SCRIPT_DIR)
  set(ONE_VALUES HEADER HEADER_DIR)
  set(MULTIPLE_VALUES FLAGS DEPENDS)
  cmake_parse_arguments(ARGS "" "${ONE_VALUES}" "${MULTIPLE_VALUES}" ${ARGN})
  string(REPLACE ";" "\n" ARGS_FLAGS "${ARGS_FLAGS}")
  string(REPLACE ";" " " ARGS_DEPENDS "${ARGS_DEPENDS}")

  # Compute the path from the root to this file.
  if(NOT ARGS_HEADER_DIR)
    cmake_path(RELATIVE_PATH CMAKE_CURRENT_SOURCE_DIR BASE_DIRECTORY ${PROJECT_SOURCE_DIR} OUTPUT_VARIABLE ARGS_HEADER_DIR)
  endif()

  cmake_path(APPEND ARGS_HEADER_DIR ${ARGS_HEADER} OUTPUT_VARIABLE HEADER_FILE)

  set(RESPONSE_FILE "${TARGET_NAME}.rsp")
  cmake_path(APPEND PROJECT_BINARY_DIR ${ARGS_HEADER_DIR} OUTPUT_VARIABLE RESPONSE_FILE_REAL_PATH)
  cmake_path(APPEND RESPONSE_FILE_REAL_PATH ${RESPONSE_FILE})
  message(${RESPONSE_FILE_REAL_PATH})
  file(WRITE ${RESPONSE_FILE_REAL_PATH} "--flags\n${ARGS_FLAGS}")

  add_custom_command(OUTPUT ${PROJECT_BINARY_DIR}/${HEADER_FILE} COMMAND python3 ${SCRIPT_DIR}/write_buildflag_header.py --output ${HEADER_FILE} --gen-dir \"\" --rulename ${TARGET_NAME} --definitions ${ARGS_HEADER_DIR}/${RESPONSE_FILE} WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
  add_custom_target(${TARGET_NAME} ALL DEPENDS ${CMAKE_CURRENT_LIST_FILE} ${PROJECT_BINARY_DIR}/${HEADER_FILE} ${ARGS_DEPENDS})

  add_dependencies(${TARGET_NAME} buildflag_header_h)
endfunction()
