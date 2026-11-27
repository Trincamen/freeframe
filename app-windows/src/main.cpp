#include <windows.h>
#include <shellapi.h>

#include <cstdlib>
#include <cwchar>
#include <string>
#include <winrt/base.h>

#include "app.hpp"
#include "log.hpp"
#include "strings.hpp"

using ffapp::Options;

namespace {

bool parseArgs(Options& o, std::wstring& error) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return true;
    bool ok = true;
    for (int i = 1; i < argc && ok; ++i) {
        const std::wstring a = argv[i];
        const bool hasValue = i + 1 < argc;
        if (a == L"--algo" && hasValue) {
            const std::wstring v = argv[++i];
            if (v == L"nearest") o.scaler.algorithm = ff::d3d11::ScaleAlgorithm::Nearest;
            else if (v == L"bilinear") o.scaler.algorithm = ff::d3d11::ScaleAlgorithm::Bilinear;
            else if (v == L"fsr1") o.scaler.algorithm = ff::d3d11::ScaleAlgorithm::Fsr1;
            else {
                error = L"Unknown algorithm: " + v;
                ok = false;
            }
        } else if (a == L"--sharpness" && hasValue) {
            const float v = static_cast<float>(_wtof(argv[++i]));
            o.scaler.sharpness = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        } else if (a == L"--target-title" && hasValue) {
            o.targetTitle = argv[++i];
        } else if (a == L"--exit-after" && hasValue) {
            o.exitAfterSeconds = _wtof(argv[++i]);
        } else if (a == L"--log" && hasValue) {
            o.logPath = argv[++i];
        } else if (a == L"--present-every-vblank") {
            o.presentEveryVblank = true;
        } else if (a == L"--no-vsync") {
            o.vsync = false;
        } else {
            error = L"Invalid argument: " + a;
            ok = false;
        }
    }
    LocalFree(argv);
    return ok;
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    Options options;
    std::wstring error;
    const bool argsOk = parseArgs(options, error);
    ffapp::Log::open(options.logPath);
    if (!argsOk) {
        ffapp::Log::write("%s", ffapp::toUtf8(error).c_str());
        MessageBoxA(nullptr, ffapp::str::kUsage, ffapp::str::kOverlayTitle, MB_OK | MB_ICONINFORMATION);
        return 2;
    }
    int code = 1;
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        ffapp::App app(std::move(options));
        code = app.run();
    } catch (const std::exception& e) {
        ffapp::Log::write("Fatal: %s", e.what());
    } catch (...) {
        ffapp::Log::write("Fatal: unknown exception");
    }
    ffapp::Log::close();
    return code;
}
