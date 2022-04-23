include(${PROJECT_SOURCE_DIR}/build/config/chromeos/ui_mode.cmake)
include(${PROJECT_SOURCE_DIR}/build/config/dcheck_always_on.cmake)

if(CHROMEOS_ASH AND NOT (CMAKE_BUILD_TYPE STREQUAL "Debug" OR DCHECK_ALWAYS_ON))
  set(ENABLE_LOG_ERROR_NOT_REACHED ON)
else()
  set(ENABLE_LOG_ERROR_NOT_REACHED OFF)
endif()

set(ENABLE_STACK_TRACE_LINE_NUMBERS OFF)
