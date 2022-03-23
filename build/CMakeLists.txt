include(config/chromeos/args.cmake)

add_custom_target(buildflag_header_h DEPENDS buildflag.h)

add_library(basium_build INTERFACE build_config.h buildflag.h)
target_include_directories(basium_build INTERFACE ${BASIUM_COMMON_INCLUDE_DIR})

include(BuildflagHeader)
buildflag_header(chromeos_buildflags
  HEADER "chromeos_buildflags.h"
  FLAGS
  IS_CHROMEOS_DEVICE=${CHROMEOS_DEVICE}

  IS_CHROMEOS_WITH_HW_DETAILS=${CHROMEOS_WITH_HW_DETAILS}
  IS_REVEN=${IS_REVEN})
