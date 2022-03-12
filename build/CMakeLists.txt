add_custom_target(buildflag_header_h DEPENDS buildflag.h)

add_library(basium_build INTERFACE build_config.h buildflag.h)
target_include_directories(basium_build INTERFACE ${BASIUM_COMMON_INCLUDE_DIR})
