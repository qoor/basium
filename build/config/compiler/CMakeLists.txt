include(${PROJECT_SOURCE_DIR}/build/config/compiler/compiler.cmake)

set(STRIP_ABSOLUTE_PATHS_FROM_DEBUG_SYMBOLS OFF CACHE BOOL "Optimize symbol files for maximizing goma cache hit rate.")

buildflag_header(compiler_buildflags
  HEADER "compiler_buildflags.h"
  FLAGS
  CLANG_PGO=OFF
  SYMBOL_LEVEL=${SYMBOL_LEVEL})
