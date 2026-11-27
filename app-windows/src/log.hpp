#pragma once
// Tiny logger: file (optional) + OutputDebugString. Single-threaded use (main thread).

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <string>

namespace ffapp {

inline std::string toUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

class Log {
public:
    static void open(const std::wstring& path) {
        close();
        if (path.empty()) return;
        _wfopen_s(&file(), path.c_str(), L"wb");
    }
    static void close() {
        if (file()) {
            std::fclose(file());
            file() = nullptr;
        }
    }
    static void write(const char* fmt, ...) {
        char body[1024];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(body, sizeof body, fmt, ap);
        va_end(ap);
        char line[1100];
        const int n = snprintf(line, sizeof line, "[%8.3f] %s\n", seconds(), body);
        if (n <= 0) return;
        OutputDebugStringA(line);
        if (file()) {
            std::fwrite(line, 1, static_cast<size_t>(n), file());
            std::fflush(file());
        }
    }

private:
    static FILE*& file() {
        static FILE* f = nullptr;
        return f;
    }
    static double seconds() {
        static const ULONGLONG start = GetTickCount64();
        return static_cast<double>(GetTickCount64() - start) / 1000.0;
    }
};

} // namespace ffapp
