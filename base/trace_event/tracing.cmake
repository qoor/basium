include(${PROJECT_SOURCE_DIR}/build/config/chromeos/ui_mode.cmake)

# Enable more trace events. Disabled by default due to binary size impact,
# but highly recommended for local development.
set(EXTENDED_TRACING_ENABLED OFF)

if((NOT ANDROID AND NOT CHROMEOS_ASH) OR EXTENDED_TRACING_ENABLED)
  set(OPTIONAL_TRACE_EVENTS_ENABLED ON)
else()
  set(OPTIONAL_TRACE_EVENTS_ENABLED OFF)
endif()
