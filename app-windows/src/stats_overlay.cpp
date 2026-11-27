#include "stats_overlay.hpp"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include "strings.hpp"

namespace ffapp {

StatsOverlay::~StatsOverlay() { shutdown(); }

bool StatsOverlay::init(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* ctx) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange;
    if (!ImGui_ImplWin32_Init(hwnd)) {
        ImGui::DestroyContext();
        return false;
    }
    if (!ImGui_ImplDX11_Init(device, ctx)) {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        return false;
    }
    m_scale = static_cast<float>(GetDpiForWindow(hwnd)) / 96.0f;
    if (m_scale < 1.0f) m_scale = 1.0f;
    io.FontGlobalScale = m_scale;
    m_ctx = ctx;
    m_initialized = true;
    return true;
}

void StatsOverlay::shutdown() {
    if (!m_initialized) return;
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    m_initialized = false;
}

void StatsOverlay::render(ID3D11RenderTargetView* rtv, UINT width, UINT height, const StatsSnapshot& st) {
    if (!m_initialized) return;
    (void)width;
    (void)height;
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(12.0f * m_scale, 12.0f * m_scale));
    ImGui::SetNextWindowBgAlpha(0.55f);
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                                       ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings |
                                       ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing;
    if (ImGui::Begin(str::kOverlayTitle, nullptr, flags)) {
        ImGui::Text(str::kBaseFps, st.baseFps);
        ImGui::Text(str::kOutFps, st.outFps);
        ImGui::Text(str::kCpuMs, st.cpuMs);
        if (st.gpuValid) ImGui::Text(str::kGpuMs, st.gpuMs[0], st.gpuMs[1], st.gpuMs[2]);
        else ImGui::TextUnformatted(str::kGpuNa);
        ImGui::Text(str::kAlgo, st.algorithm);
    }
    ImGui::End();
    ImGui::Render();

    m_ctx->OMSetRenderTargets(1, &rtv, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

} // namespace ffapp
