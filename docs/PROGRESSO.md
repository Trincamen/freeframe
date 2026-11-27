# Progresso do FreeFrame

## Marco atual: M0 — Esqueleto (concluído no Windows; falta CI/Linux)

### Tarefas
| # | Tarefa | Agente | Arquivos | Status |
|---|---|---|---|---|
| T1 | LICENSE (MIT), .gitignore, .clang-format, .editorconfig, THIRD_PARTY_NOTICES.md, README bilíngue | Haiku | raiz (exceto CMake), README.md | concluída (revisada pelo Opus) |
| T2 | CMakePresets.json + CI GitHub Actions (Windows MSVC + Ubuntu Clang/GCC) | Haiku | CMakePresets.json, .github/ | concluída (Opus adicionou `permissions: contents: read`) |
| T3 | CMake raiz + core (`IGpuBackend`, Vulkan via volk, DXGI) + `freeframe-cli` + testes doctest | Sonnet | CMakeLists.txt, cmake/, core/, cli/, tests/ | concluída |
| T3b | Correções: `toString`→`to_string` (conflito ADL com doctest); dedupe DXGI por LUID | Haiku | core/, cli/, tests/ | concluída |
| T4 | Revisão + integração + build local | Opus | — | concluída |
| T5 | CI verde no GitHub (Windows + Ubuntu) | — | — | **pendente: repositório no GitHub** |

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
- Linux ainda não compilado (será coberto pelo CI ou pelo PC Ubuntu do usuário).

### Decisões
Ver PROJETO.md, seção 14 (D1–D6).

### Pendências do usuário
- [x] VS Build Tools 2022 instalado (winget).
- [x] Vulkan SDK 1.4.363.0 instalado em `C:\VulkanSDK` (inclui dxc, glslc, validation layers).
- [ ] Criar repositório no GitHub e autorizar o push, para o CI rodar (Windows + Ubuntu).
- [ ] (Opcional) Compilar no Ubuntu: `cmake --preset linux-clang && cmake --build --preset linux-clang && ctest --preset linux-clang`.
