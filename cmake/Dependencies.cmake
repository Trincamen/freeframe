include(FetchContent)

# Older third-party projects may declare a cmake_minimum_required that CMake 4+ rejects.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# --- Vulkan-Headers (no Vulkan SDK needed at build time) ---
FetchContent_Declare(
  vulkan_headers
  GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
  GIT_TAG        vulkan-sdk-1.4.309.0
  GIT_SHALLOW    TRUE
  SYSTEM
)

# --- volk (loads the Vulkan loader at runtime) ---
FetchContent_Declare(
  volk
  GIT_REPOSITORY https://github.com/zeux/volk.git
  GIT_TAG        vulkan-sdk-1.4.309.0
  GIT_SHALLOW    TRUE
  SYSTEM
)

FetchContent_MakeAvailable(vulkan_headers)

# We link Vulkan::Headers to volk ourselves instead of letting it search for an SDK.
set(VOLK_PULL_IN_VULKAN OFF CACHE BOOL "" FORCE)
set(VOLK_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(volk)
if(TARGET volk)
  target_link_libraries(volk PUBLIC Vulkan::Headers)
endif()

# --- doctest ---
if(FREEFRAME_BUILD_TESTS)
  set(DOCTEST_WITH_TESTS OFF CACHE BOOL "" FORCE)
  set(DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF CACHE BOOL "" FORCE)
  set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)
  FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG        v2.4.11
    GIT_SHALLOW    TRUE
    SYSTEM
  )
  FetchContent_MakeAvailable(doctest)
endif()

# --- AMD FidelityFX FSR 1 (MIT): headers only (ffx_a.h, ffx_fsr1.h), used by fxc and by the C++ constant setup ---
if(WIN32)
  FetchContent_Declare(
    fidelityfx_fsr
    GIT_REPOSITORY https://github.com/GPUOpen-Effects/FidelityFX-FSR.git
    GIT_TAG        a21ffb8f6c13233ba336352bdff293894c706575 # v1.0.2
    SYSTEM
  )
  FetchContent_MakeAvailable(fidelityfx_fsr) # the repository has no root CMakeLists.txt: populate only
  set(FREEFRAME_FSR_INCLUDE_DIR "${fidelityfx_fsr_SOURCE_DIR}/ffx-fsr" CACHE INTERNAL "")
endif()
