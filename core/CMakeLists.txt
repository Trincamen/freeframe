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
  target_sources(freeframe_core PRIVATE src/dxgi_backend.cpp)
endif()
add_library(FreeFrame::core ALIAS freeframe_core)

target_include_directories(freeframe_core PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_features(freeframe_core PUBLIC cxx_std_20)
target_link_libraries(freeframe_core PRIVATE volk)
if(WIN32)
  target_link_libraries(freeframe_core PRIVATE dxgi)
  target_compile_definitions(freeframe_core PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
freeframe_set_warnings(freeframe_core)
