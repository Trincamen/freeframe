configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/include/freeframe/version.hpp.in"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/freeframe/version.hpp"
  @ONLY)

add_library(freeframe_core STATIC
  src/gpu_backend.cpp
  src/format.cpp
  src/vulkan_backend.cpp
)
if(WIN32)
  target_sources(freeframe_core PRIVATE src/dxgi_backend.cpp src/d3d11/device.cpp
    src/d3d11/scaler.cpp
    src/d3d11/gpu_timer.cpp
    src/d3d11/fsr_constants.cpp)
  include(Shaders)
  set(_ffs "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
  freeframe_add_compute_shader(freeframe_core "${_ffs}/resample.hlsl" g_shader_resample_cs)
  freeframe_add_compute_shader(freeframe_core "${_ffs}/fsr_easu.hlsl" g_shader_fsr_easu_cs)
  freeframe_add_compute_shader(freeframe_core "${_ffs}/fsr_rcas.hlsl" g_shader_fsr_rcas_cs)
endif()
add_library(FreeFrame::core ALIAS freeframe_core)

target_include_directories(freeframe_core PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_features(freeframe_core PUBLIC cxx_std_20)
target_link_libraries(freeframe_core PRIVATE volk)
if(WIN32)
  target_link_libraries(freeframe_core PRIVATE dxgi PUBLIC d3d11)
  target_compile_definitions(freeframe_core PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
  target_include_directories(freeframe_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/shaders")
  # ffx_a.h / ffx_fsr1.h (AMD, MIT) are only needed by the CPU constant setup.
  target_include_directories(freeframe_core SYSTEM PRIVATE "${FREEFRAME_FSR_INCLUDE_DIR}")
endif()
freeframe_set_warnings(freeframe_core)
