# Copyright 2022 MaBling <akck0918@gmail.com>. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include(${PROJECT_SOURCE_DIR}/build/config/chromeos/ui_mode.cmake)

# Enable more trace events. Disabled by default due to binary size impact,
# but highly recommended for local development.
set(EXTENDED_TRACING_ENABLED OFF)

if((NOT ANDROID AND NOT CHROMEOS_ASH) OR EXTENDED_TRACING_ENABLED)
  set(OPTIONAL_TRACE_EVENTS_ENABLED ON)
else()
  set(OPTIONAL_TRACE_EVENTS_ENABLED OFF)
endif()
