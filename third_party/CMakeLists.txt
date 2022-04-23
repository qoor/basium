if(NOT TARGET absl::base)
  add_subdirectory(abseil-cpp)
endif()

add_subdirectory(googletest)
add_subdirectory(openssl)
add_subdirectory(modp_b64)
add_subdirectory(icu)
add_subdirectory(ced)
