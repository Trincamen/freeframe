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

void StatsOverlay::render(ID3D11RenderTargetView* rtv, UINT width, UINT height, const StatsSnapshot& st, bool showStats,
                           const CursorDraw* cursor) {
    if (!m_initialized) return;
    (void)height;
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Toasts (top center), independent of the statistics visibility.
    if (!st.toastSettings.empty() || !st.toastTip.empty()) {
        ImGui::SetNextWindowPos(ImVec2(static_cast<float>(width) * 0.5f, 12.0f * m_scale), ImGuiCond_Always,
                                ImVec2(0.5f, 0.0f));
        ImGui::SetNextWindowBgAlpha(0.75f);
        constexpr ImGuiWindowFlags tf = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                                        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings |
                                        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing;
        if (ImGui::Begin("##ffToast", nullptr, tf)) {
            if (!st.toastSettings.empty()) ImGui::TextUnformatted(st.toastSettings.c_str());
            if (!st.toastTip.empty()) {
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 520.0f * m_scale);
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "%s", st.toastTip.c_str());
                ImGui::PopTextWrapPos();
            }
        }
        ImGui::End();
    }
    if (showStats) {
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
        if (st.srcW && st.srcH && st.dstW && st.dstH) {
            const double factor = static_cast<double>(st.dstW) / static_cast<double>(st.srcW);
            if (st.srcW == st.dstW && st.srcH == st.dstH) {
                // No scaling: show the resolution line in yellow with a hint.
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.2f, 1.0f));
                ImGui::Text(str::kScale, st.srcW, st.srcH, st.dstW, st.dstH, st.algorithm, factor);
                ImGui::SameLine(0.0f, 0.0f);
                ImGui::TextUnformatted(str::kNoUpscaling);
                ImGui::PopStyleColor();
            } else {
                ImGui::Text(str::kScale, st.srcW, st.srcH, st.dstW, st.dstH, st.algorithm, factor);
            }
        }
        if (!st.tips.empty()) {
            ImGui::Separator();
            ImGui::TextUnformatted(str::kTipsHeader);
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 520.0f * m_scale);
            int shown = 0;
            for (const ff::Tip& tip : st.tips) {
                if (shown++ >= 3) break;
                ImVec4 col(0.7f, 0.7f, 0.7f, 1.0f);
                if (tip.severity == ff::TipSeverity::Warning) col = ImVec4(1.0f, 0.65f, 0.2f, 1.0f);
                else if (tip.severity == ff::TipSeverity::Suggestion) col = ImVec4(0.55f, 0.8f, 1.0f, 1.0f);
                ImGui::TextColored(col, "- %s", tip.text.c_str());
            }
            ImGui::PopTextWrapPos();
        }
    }
    ImGui::End();
    }
    if (cursor && cursor->srv) {
        ImGui::GetForegroundDrawList()->AddImage(reinterpret_cast<ImTextureID>(cursor->srv),
                                                 ImVec2(cursor->x, cursor->y),
                                                 ImVec2(cursor->x + cursor->w, cursor->y + cursor->h));
    }
    ImGui::Render();

    m_ctx->OMSetRenderTargets(1, &rtv, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

} // namespace ffapp
