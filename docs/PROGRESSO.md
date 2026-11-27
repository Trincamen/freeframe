# Progresso do FreeFrame

## Marco atual: M1 — Mini-Magpie no Windows (em andamento)

Contrato núcleo ↔ app: `core/include/freeframe/d3d11/scaler.hpp` (commit 8074752).

| # | Tarefa | Agente | Onde | Status |
|---|---|---|---|---|
| S1 | Backend D3D11: device, scaler Nearest/Bilinear/FSR1 (EASU+RCAS), GpuTimer, shaders FXC embutidos, `freeframe-cli bench-scale`, testes | Sonnet | worktree `../FreeFrame-s1`, branch `m1-s1-core` | em andamento |
| S2 | `FreeFrame.exe`: captura WGC, overlay em tela cheia, swapchain flip+UAV, hotkeys Ctrl+Alt+S/O, estatísticas ImGui, log | Sonnet | worktree `../FreeFrame-s2`, branch `m1-s2-app` | em andamento |
| I1 | Merge S1+S2, remover fallbacks TEMP, build/CI | Opus | main | pendente |
| U1 | Teste com jogo real 1280x720 → tela cheia na RTX 4070 (aceite) | Usuário | — | pendente |

Nota: o isolamento automático de worktree do Claude Code recusou o caminho (diferença `c:` vs `C:`); worktrees criadas manualmente com `git worktree add`.

---

## M0 — Esqueleto (concluído 2026-11-27)

### Tarefas
| # | Tarefa | Agente | Arquivos | Status |
|---|---|---|---|---|
| T1 | LICENSE (MIT), .gitignore, .clang-format, .editorconfig, THIRD_PARTY_NOTICES.md, README bilíngue | Haiku | raiz (exceto CMake), README.md | concluída (revisada pelo Opus) |
| T2 | CMakePresets.json + CI GitHub Actions (Windows MSVC + Ubuntu Clang/GCC) | Haiku | CMakePresets.json, .github/ | concluída (Opus adicionou `permissions: contents: read`) |
| T3 | CMake raiz + core (`IGpuBackend`, Vulkan via volk, DXGI) + `freeframe-cli` + testes doctest | Sonnet | CMakeLists.txt, cmake/, core/, cli/, tests/ | concluída |
| T3b | Correções: `toString`→`to_string` (conflito ADL com doctest); dedupe DXGI por LUID | Haiku | core/, cli/, tests/ | concluída |
| T4 | Revisão + integração + build local | Opus | — | concluída |
| T5 | CI no GitHub (Windows + Ubuntu) | Opus | — | Linux clang/gcc OK; Windows corrigido (runner windows-2022) |

### Resultado local (Windows, MSVC 19.44, `/W4 /WX`)
- Build Release: OK, 0 warnings.
- `ctest`: 10 casos / 34 asserções, 100% OK.
- `dumpbin /dependents freeframe-cli.exe`: só `dxgi.dll` e `KERNEL32.dll` (runtime estático OK).
- `freeframe-cli gpus`: lista **NVIDIA GeForce RTX 4070** (Vulkan 1.4.357, driver 621.40) → **aceite do M0 atingido**.

### Observações
- O DXGI enumera a RTX 4070 **duas vezes com LUIDs diferentes** (mesmo vendor/device/subsys). Não é bug nosso:
  é o que `EnumAdapters1` devolve nesta máquina (Windows Insider 26300; pode ser driver/monitor virtual).
  Decisão: **não** deduplicar por identidade PCI (esconderia duas GPUs idênticas reais, relevante para Dual GPU).
  No M1, o adaptador D3D11 será escolhido casando o `deviceLUID` do Vulkan (`VkPhysicalDeviceIDProperties`)
  ou por `IDXGIFactory6::EnumAdapterByGpuPreference`.
- Linux: build + testes verdes no CI (Ubuntu 24.04, clang e gcc).

### Decisões
Ver PROJETO.md, seção 14 (D1–D6).

### Pendências do usuário
- [x] VS Build Tools 2022 instalado (winget).
- [x] Vulkan SDK 1.4.363.0 instalado em `C:\VulkanSDK` (inclui dxc, glslc, validation layers).
- [x] Repositório público: https://github.com/Trincamen/freeframe
- [ ] (Opcional) Compilar no Ubuntu: `cmake --preset linux-clang && cmake --build --preset linux-clang && ctest --preset linux-clang`.
