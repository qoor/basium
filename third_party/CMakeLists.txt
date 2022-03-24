if(NOT TARGET absl::base)
  add_subdirectory(abseil-cpp)
endif()

add_subdirectory(googletest)
