# Dear ImGui (docking-less release tag) compiled as a separate static library, WITHOUT FreeFrame warning flags.
include(FetchContent)

FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        v1.91.9
  GIT_SHALLOW    TRUE
  SYSTEM
)
FetchContent_MakeAvailable(imgui)

if(NOT TARGET freeframe_imgui)
  add_library(freeframe_imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp")
  target_include_directories(freeframe_imgui SYSTEM PUBLIC
    "${imgui_SOURCE_DIR}" "${imgui_SOURCE_DIR}/backends")
  target_compile_definitions(freeframe_imgui PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
  target_link_libraries(freeframe_imgui PUBLIC d3d11 dxgi dwmapi)
  add_library(FreeFrame::imgui ALIAS freeframe_imgui)
endif()
