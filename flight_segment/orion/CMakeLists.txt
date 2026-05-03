####
# This sets up the build system for the 'Orion' project, including
# components and deployments from project.cmake. In addition, it imports the core F Prime components.
####

cmake_minimum_required(VERSION 3.24.2)
project(Orion C CXX)

###
# F' Core Setup
# This includes all of the F prime core components, and imports the make-system.
###
include("${CMAKE_CURRENT_LIST_DIR}/lib/fprime/cmake/FPrime.cmake")
# NOTE: register custom targets between these two lines
fprime_setup_included_code()

# Override F-Prime's default FpConstants.fpp with project-level values.
# Bumps FW_COM_BUFFER_MAX_SIZE, FW_LOG_STRING_MAX_SIZE, and
# FW_FIXED_LENGTH_STRING_SIZE so VLM reason strings are not truncated.
register_fprime_config(
    orion_config
    CONFIGURATION_OVERRIDES
        "${CMAKE_CURRENT_LIST_DIR}/config/FpConstants.fpp"
    INTERFACE
)

# This includes project-wide objects
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Orion")

# Register the Orion deployment: produces the 'Orion' executable
register_fprime_deployment(
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/Orion/Main.cpp"
    DEPENDS
        Orion_Top
)
