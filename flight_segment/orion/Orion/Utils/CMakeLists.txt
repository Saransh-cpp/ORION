set(SOURCE_FILES
  "${CMAKE_CURRENT_LIST_DIR}/SimSatClient.cpp"
)

# SimSatClient is a plain C++ utility, not an F-Prime component.
# It wraps libcurl and uses stb vendor headers for PNG decode/resize.

find_package(CURL REQUIRED)

register_fprime_module()

target_link_libraries(
  ${MODULE_NAME}
  PUBLIC CURL::libcurl
)

target_include_directories(
  ${MODULE_NAME}
  PUBLIC "${CMAKE_CURRENT_LIST_DIR}/.."
)
