set(SOURCE_FILES
  "${CMAKE_CURRENT_LIST_DIR}/EventAction.fpp"
  "${CMAKE_CURRENT_LIST_DIR}/EventAction.cpp"
)

set(MOD_DEPS
  Orion_Types
  Orion_Ports
)

register_fprime_module()
