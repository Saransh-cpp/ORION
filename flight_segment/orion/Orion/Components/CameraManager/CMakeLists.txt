set(SOURCE_FILES
  "${CMAKE_CURRENT_LIST_DIR}/CameraManager.fpp"
  "${CMAKE_CURRENT_LIST_DIR}/CameraManager.cpp"
)

set(MOD_DEPS
  Orion_Types
  Orion_Ports
  Orion_Utils
)

register_fprime_module()
