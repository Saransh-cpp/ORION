# Resolve the llama.cpp repo root (5 levels up from this file).
# Layout: ORION/flight_segment/orion/Orion/Components/VlmInferenceEngine/
get_filename_component(ORION_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../../../../../"
    ABSOLUTE)

set(LLAMA_ROOT  "${ORION_ROOT}/ground_segment/llama.cpp")
set(LLAMA_BUILD "${LLAMA_ROOT}/build"
    CACHE PATH "Path to the llama.cpp CMake build directory (run cmake+make there first)")

set(SOURCE_FILES
    "${CMAKE_CURRENT_LIST_DIR}/VlmInferenceEngine.fpp"
    "${CMAKE_CURRENT_LIST_DIR}/VlmInferenceEngine.cpp"
)

# llama.cpp public headers required at compile time.
include_directories(
    "${LLAMA_ROOT}/include"
    "${LLAMA_ROOT}/ggml/include"
    "${LLAMA_ROOT}/tools/mtmd"
)

set(MOD_DEPS
    Orion_Types
    Orion_Ports
)

register_fprime_module()

# Link against pre-built llama.cpp static libraries.
# Build order: cd ground_segment/llama.cpp && cmake -B build -DBUILD_SHARED_LIBS=OFF && cmake --build build
target_link_libraries(Orion_Components_VlmInferenceEngine PRIVATE
    "${LLAMA_BUILD}/src/libllama.a"
    "${LLAMA_BUILD}/tools/mtmd/libmtmd.a"
    "${LLAMA_BUILD}/ggml/src/libggml.a"
    "${LLAMA_BUILD}/ggml/src/libggml-base.a"
    "${LLAMA_BUILD}/ggml/src/libggml-cpu.a"
)

# macOS: Metal GPU + Accelerate (vDSP) on Apple Silicon
if(APPLE)
    target_link_libraries(Orion_Components_VlmInferenceEngine PRIVATE
        "${LLAMA_BUILD}/ggml/src/ggml-blas/libggml-blas.a"
        "${LLAMA_BUILD}/ggml/src/ggml-metal/libggml-metal.a"
        "-framework Accelerate"
        "-framework Metal"
        "-framework Foundation"
    )
endif()

# Linux/Pi 5: OpenMP for CPU parallelism
if(UNIX AND NOT APPLE)
    target_link_libraries(Orion_Components_VlmInferenceEngine PRIVATE gomp)
endif()
