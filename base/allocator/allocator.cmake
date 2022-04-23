# Copyright 2022 MaBling <akck0918@gmail.com>. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include(${PROJECT_SOURCE_DIR}/build/config/sanitizers/sanitizers.cmake)

if(ASAN OR HWASAN OR LSAN OR TSAN OR MSAN)
  set(USING_SANITIZERS ON)
else()
  set(USING_SANITIZERS OFF)
endif()

if(BUILD_SHARED_LIBS OR (WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Debug"))
  set(DISABLE_PARTITION_ALLOC ON)
else()
  set(DISABLE_PARTITION_ALLOC OFF)
endif()

if(NOT IOS)
  set(PARTITION_ALLOC_PLATFORM ON)
else()
  set(PARTITION_ALLOC_PLATFORM OFF)
endif()

if((LINUX OR CHROMEOS OR ANDROID OR APPLE OR (WIN32 AND NOT BUILD_SHARED_LIBS AND NOT CMAKE_BUILD_TYPE STREQUAL DEBUG)) AND NOT USING_SANITIZERS)
  set(DEFAULT_USE_ALLOCATOR_SHIM ON)
else()
  set(DEFAULT_USE_ALLOCATOR_SHIM OFF)
endif()

if(DEFAULT_USE_ALLOCATOR_SHIM AND PARTITION_ALLOC_PLATFORM AND NOT DISABLE_PARTITION_ALLOC)
  set(DEFAULT_ALLOCATOR "partition")
else()
  set(DEFAULT_ALLOCATOR "none")
endif()

set(USE_ALLOCATOR ${DEFAULT_ALLOCATOR})
set(USE_ALLOCATOR_SHIM ${DEFAULT_USE_ALLOCATOR_SHIM})
set(USE_PARTITION_ALLOC ON)

if(NOT USE_PARTITION_ALLOC AND USE_ALLOCATOR STREQUAL "partition")
  set(USE_ALLOCATOR "none")
endif()

if(NOT USE_ALLOCATOR STREQUAL "none" AND NOT USE_ALLOCATOR STREQUAL "partition")
  message(FATAL_ERROR "Invalid USE_ALLOCATOR")
endif()

if(USE_ALLOCATOR_SHIM AND NOT LINUX AND NOT CHROMEOS AND NOT ANDROID AND NOT WIN32 AND NOT APPLE)
  message(FATAL_ERROR "USE_ALLOCATOR_SHIM works only on Android, iOS, Linux, macOS, and Windows")
endif()

if(WIN32 AND USE_ALLOCATOR_SHIM)
  if(BUILD_SHARED_LIBS)
    message(FATAL_ERROR "The allocator shim doesn't work for the shared library build on Windows.")
  endif()
endif()

if((WIN32 OR ANDROID) AND USE_ALLOCATOR STREQUAL "partition")
  set(BRP_SUPPORTED ON)
else()
  set(BRP_SUPPORTED OFF)
endif()

set(USE_BACKUP_REF_PTR ${BRP_SUPPORTED})

set(PUT_REF_COUNT_IN_PREVIOUS_SLOT ${USE_BACKUP_REF_PTR})
set(NEVER_REMOVE_FROM_BRP_POOL_BLOCKLIST ${USE_BACKUP_REF_PTR})
set(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS OFF)
set(ENABLE_DANGLING_RAW_PTR_CHECKS OFF)

set(USE_FAKE_BINARY_EXPERIMENT OFF)

set(USE_ASAN_BACKUP_REF_PTR OFF)

if(USE_BACKUP_REF_PTR AND NOT USE_ALLOCATOR STREQUAL "partition")
  message(FATAL_ERROR "Can't use BackupRefPtr without PartitionAlloc-Everywhere")
endif()

if(NOT USE_BACKUP_REF_PTR AND PUT_REF_COUNT_IN_PREVIOUS_SLOT)
  message(FATAL_ERROR "Can't put ref count in the previous slot if BackupRefPtr isn't enabled at all")
endif()

if(NOT USE_BACKUP_REF_PTR AND NEVER_REMOVE_FROM_BRP_POOL_BLOCKLIST)
  message(FATAL_ERROR "NEVER_REMOVE_FROM_BRP_POOL_BLOCKLIST requires BackupRefPtr to be enabled")
endif()

if(NOT USE_BACKUP_REF_PTR AND ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
  message(FATAL-ERROR "Can't enable additional BackupRefPtr checks if it isn't enabled at all")
endif()

if(NOT USE_BACKUP_REF_PTR AND ENABLE_DANGLING_RAW_PTR_CHECKS)
  message(FATAL_ERROR "Can't enable dangling raw_ptr checks if BackupRefPtr isn't enabled at all")
endif()

if(USE_BACKUP_REF_PTR AND USE_ASAN_BACKUP_REF_PTR)
  message(FATAL_ERROR "Both BackupRefPtr and AsanBackupRefPtr can't be enabled at the same time")
endif()

if(USE_ASAN_BACKUP_REF_PTR AND NOT ASAN)
  message(FATAL_ERROR "AsanBackupRefPtr requires AddressSanitizer")
endif()
