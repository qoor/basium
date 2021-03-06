# Copyright 2022 MaBling <akck0918@gmail.com>. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include(${PROJECT_SOURCE_DIR}/base/allocator/allocator.cmake)
include(${PROJECT_SOURCE_DIR}/build/BuildflagHeader.cmake)
include(${PROJECT_SOURCE_DIR}/build/config/compiler/compiler.cmake)
include(${PROJECT_SOURCE_DIR}/build/config/dcheck_always_on.cmake)

if(USE_ALLOCATOR STREQUAL "partition")
  set(USE_PARTITION_ALLOC_AS_MALLOC ON)
else()
  set(USE_PARTITION_ALLOC_AS_MALLOC OFF)
endif()
if(NOT USE_ALLOCATOR_SHIM AND USE_PARTITION_ALLOC_AS_MALLOC)
  message(FATAL_ERROR "Partition alloc requires the allocator shim")
endif()

if(USE_BACKUP_REF_PTR AND USE_PARTITION_ALLOC)
  set(USE_BACKUP_REF_PTR ON)
else()
  set(USE_BACKUP_REF_PTR OFF)
endif()
if(PUT_REF_COUNT_IN_PREVIOUS_SLOT AND USE_BACKUP_REF_PTR)
  set(PUT_REF_COUNT_IN_PREVIOUS_SLOT ON)
else()
  set(PUT_REF_COUNT_IN_PREVIOUS_SLOT OFF)
endif()
if(NEVER_REMOVE_FROM_BRP_POOL_BLOCKLIST AND USE_BACKUP_REF_PTR)
  set(NEVER_REMOVE_FROM_BRP_POOL_BLOCKLIST ON)
else()
  set(NEVER_REMOVE_FROM_BRP_POOL_BLOCKLIST OFF)
endif()
if(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS AND USE_BACKUP_REF_PTR)
  set(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS ON)
else()
  set(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS OFF)
endif()
if(ENABLE_DANGLING_RAW_PTR_CHECKS AND USE_BACKUP_REF_PTR)
  set(ENABLE_DANGLING_RAW_PTR_CHECKS ON)
else()
  set(ENABLE_DANGLING_RAW_PTR_CHECKS OFF)
endif()

set(RECORD_ALLOC_INFO OFF)

buildflag_header(allocator_buildflags
  HEADER "buildflags.h"

  FLAGS
  USE_ALLOCATOR_SHIM=${USE_ALLOCATOR_SHIM}
  USE_PARTITION_ALLOC=${USE_PARTITION_ALLOC}
  USE_PARTITION_ALLOC_AS_MALLOC=${USE_PARTITION_ALLOC_AS_MALLOC}

  USE_BACKUP_REF_PTR=${USE_BACKUP_REF_PTR}
  USE_ASAN_BACKUP_REF_PTR=${USE_ASAN_BACKUP_REF_PTR}
  ENABLE_BACKUP_REF_PTR_SLOW_CHECKS=${ENABLE_BACKUP_REF_PTR_SLOW_CHECKS}
  ENABLE_DANGLING_RAW_PTR_CHECKS=${ENABLE_DANGLING_RAW_PTR_CHECKS}
  PUT_REF_COUNT_IN_PREVIOUS_SLOT=${PUT_REF_COUNT_IN_PREVIOUS_SLOT}
  NEVER_REMOVE_FROM_BRP_POOL_BLOCKLIST=${NEVER_REMOVE_FROM_BRP_POOL_FROM_BLOCKLIST}

  USE_FAKE_BINARY_EXPERIMENT=${USE_FAKE_BINARY_EXPERIMENT}

  RECORD_ALLOC_INFO=${RECORD_ALLOC_INFO})

if(APPLE)
  add_library(basium_early_zone_registration_mac MODULE
    early_zone_registration_mac.cc
    early_zone_registration_mac.h)
  target_include_directories(basium_early_zone_registration_mac PUBLIC ${BASIUM_COMMON_INCLUDE_DIR})
  add_dependencies(basium_early_zone_registration_mac allocator_buildflags)
endif()
