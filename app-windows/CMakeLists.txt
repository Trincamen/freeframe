include(AppDependencies)

add_executable(FreeFrame WIN32
  src/main.cpp
  src/app.cpp
  src/capture_wgc.cpp
  src/overlay_window.cpp
  src/swapchain.cpp
  src/hotkeys.cpp
  src/stats_overlay.cpp
  src/cursor_map.cpp
  FreeFrame.manifest)
target_link_libraries(FreeFrame PRIVATE FreeFrame::core FreeFrame::imgui
  d3d11 dxgi dwmapi windowsapp Magnification)
target_compile_definitions(FreeFrame PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
freeframe_set_warnings(FreeFrame)
