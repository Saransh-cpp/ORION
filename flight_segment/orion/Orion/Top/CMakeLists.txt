add_compile_options(
  -Wno-shadow
)

register_fprime_module(
  AUTOCODER_INPUTS
    "${CMAKE_CURRENT_LIST_DIR}/instances.fpp"
    "${CMAKE_CURRENT_LIST_DIR}/topology.fpp"
  SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/OrionTopology.cpp"
  DEPENDS
    Fw_Logger
    Svc_PosixTime
)
