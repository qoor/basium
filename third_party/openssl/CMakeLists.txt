# Copyright 2022 MaBling <akck0918@gmail.com>. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.11)

if(UNIX)
  include(ProcessorCount)
  processorcount(NUM_PROCESSORS)
  set(CONFIGURE_COMMAND <SOURCE_DIR>/Configure)
  set(BUILD_COMMAND make -j${NUM_PROCESSORS})
  set(INSTALL_COMMAND make -j${NUM_PROCESSORS} install)
elseif(WIN32)
  set(CONFIGURE_COMMAND perl Configure)
  set(BUILD_COMMAND nmake)
  set(INSTALL_COMMAND nmake install)
else()
  message(FATAL_ERROR "This system is not supported yet OpenSSL")
endif()

set(openssl_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(openssl_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/src/include ${openssl_SOURCE_DIR}/include)
set(openssl_LIBRARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/src)

set(CRYPTO_SHARED_LIBRARY_PATH ${openssl_LIBRARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}crypto${CMAKE_SHARED_LIBRARY_SUFFIX})
set(CRYPTO_STATIC_LIBRARY_PATH ${openssl_LIBRARY_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}crypto${CMAKE_STATIC_LIBRARY_SUFFIX})
set(SSL_SHARED_LIBRARY_PATH ${openssl_LIBRARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}ssl${CMAKE_SHARED_LIBRARY_SUFFIX})
set(SSL_STATIC_LIBRARY_PATH ${openssl_LIBRARY_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}ssl${CMAKE_STATIC_LIBRARY_SUFFIX})

file(MAKE_DIRECTORY ${openssl_INCLUDE_DIR})

include(ExternalProject)
ExternalProject_Add(
  openssl
  GIT_REPOSITORY https://github.com/openssl/openssl.git
  GIT_TAG origin/openssl-3.0
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}
  SOURCE_DIR ${openssl_SOURCE_DIR}
  BINARY_DIR src
  INSTALL_DIR ${CMAKE_BINARY_DIR}
  UPDATE_DISCONNECTED ON
  CONFIGURE_HANDLED_BY_BUILD ON
  CONFIGURE_COMMAND ${CONFIGURE_COMMAND} --prefix=<INSTALL_DIR>
  BUILD_COMMAND ${BUILD_COMMAND}
  INSTALL_COMMAND ${INSTALL_COMMAND}
  BUILD_ALWAYS ON
  BUILD_BYPRODUCTS
  ${CRYPTO_SHARED_LIBRARY_PATH}
  ${CRYPTO_STATIC_LIBRARY_PATH}
  ${SSL_SHARED_LIBRARY_PATH}
  ${SSL_STATIC_LIBRARY_PATH}
  LOG_INSTALL OFF)

add_library(crypto SHARED IMPORTED GLOBAL)
add_library(crypto_static STATIC IMPORTED GLOBAL)
set_target_properties(crypto PROPERTIES IMPORTED_LOCATION ${CRYPTO_SHARED_LIBRARY_PATH})
set_target_properties(crypto_static PROPERTIES IMPORTED_LOCATION ${CRYPTO_STATIC_LIBRARY_PATH})
target_include_directories(crypto INTERFACE ${openssl_INCLUDE_DIR})
target_include_directories(crypto_static INTERFACE ${openssl_INCLUDE_DIR})
add_dependencies(crypto openssl)
add_dependencies(crypto_static openssl)

add_library(ssl SHARED IMPORTED GLOBAL)
add_library(ssl_static STATIC IMPORTED GLOBAL)
set_target_properties(ssl PROPERTIES IMPORTED_LOCATION ${SSL_SHARED_LIBRARY_PATH})
set_target_properties(ssl_static PROPERTIES IMPORTED_LOCATION ${SSL_STATIC_LIBRARY_PATH})
target_include_directories(ssl INTERFACE ${openssl_INCLUDE_DIR})
target_include_directories(ssl_static INTERFACE ${openssl_INCLUDE_DIR})
add_dependencies(ssl openssl)
add_dependencies(ssl_static openssl)
