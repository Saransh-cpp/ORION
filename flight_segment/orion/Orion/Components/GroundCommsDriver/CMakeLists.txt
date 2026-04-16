set(SOURCE_FILES
  "${CMAKE_CURRENT_LIST_DIR}/GroundCommsDriver.fpp"
  "${CMAKE_CURRENT_LIST_DIR}/GroundCommsDriver.cpp"
)

set(MOD_DEPS
  Orion_Types
  Orion_Ports
)

register_fprime_module()
