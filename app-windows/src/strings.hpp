#pragma once
// User-visible strings, isolated for a future i18n pass (English only for now).

namespace ffapp::str {

inline constexpr const char* kOverlayTitle = "FreeFrame";
inline constexpr const char* kBaseFps = "Capture FPS : %.1f";
inline constexpr const char* kOutFps = "Output FPS  : %.1f";
inline constexpr const char* kCpuMs = "CPU frame   : %.2f ms";
inline constexpr const char* kGpuMs = "GPU C/S/O   : %.2f / %.2f / %.2f ms";
inline constexpr const char* kGpuNa = "GPU         : n/a";
inline constexpr const char* kAlgo = "Algorithm   : %s";
inline constexpr const char* kScale = "%ux%u -> %ux%u (%s, %.2fx)";
inline constexpr const char* kNoUpscaling = " (no upscaling)";

inline constexpr const char* kUsage =
    "FreeFrame.exe [--algo nearest|bilinear|fsr1] [--sharpness 0..1] [--target-title <text>]\n"
    "              [--exit-after <seconds>] [--log <file>] [--no-vsync] [--present-every-vblank]\n"
    "              [--no-cursor-map] [--cursor-scale <f>]\n"
    "Hotkeys: Ctrl+Alt+S start/stop scaling the foreground window, Ctrl+Alt+O toggle statistics, Ctrl+Alt+Q quit.";

} // namespace ffapp::str
