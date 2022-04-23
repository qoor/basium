# Copyright 2022 MaBling <akck0918@gmail.com>. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Tracing support requires ${PROJECT_SOURCE_DIR}/third_party/perfetto.
# This flag can disable tracing support
# altogether, in which case all tracing instrumentation in
# ${PROJECT_SOURCE_DIR}/base becomes a no-op
set(ENABLE_BASE_TRACING OFF)

# Switches the TRACE_EVENT instrumentation from base's TraceLog implementation
# to ${PROJECT_SOURCE_DIR}/third_party/perfetto's client library.
# Not implemented yet, currently a no-op to set up trybot infrastructure.
set(USE_PERFETTO_CLIENT_LIBRARY OFF)
