# FreeFrame

[English](#english) | [Português](#português)

---

## English

Free, open-source upscaling and frame generation for any game or window — Windows & Linux.

![CI](https://github.com/Trincamen/freeframe/actions/workflows/ci.yml/badge.svg)

**Status:** 🚧 Early development — milestone M1 (overlay upscaling on Windows)

### Features (planned)

- Spatial upscaling: FSR 1, NIS, SGSR, CAS, Integer, Nearest, Bilinear, Anime4K, xBR
- Frame generation X2 / X3 / X4 and adaptive, without game support
- **Background detection of games with automatic per-game profiles** (the main differentiator)
- Overlay mode (Windows, Windows Graphics Capture) and Vulkan layer mode (Linux, Proton)
- Per-game statistics (before / after)
- 100% portable

### How it works

- **Mode A — Overlay:** captures the target window and displays the result in its own window.
- **Mode B — Vulkan layer:** a Vulkan layer that processes frames inside the game's process (Linux).

### Limitations

- Frame generation adds about one base frame of latency. It is not suitable for competitive games.
- A base framerate of at least 40–60 FPS is recommended for frame generation.
- Exclusive fullscreen is not supported in overlay mode.
- Anti-cheat: overlay mode does not inject anything into the game. The Vulkan layer may be blocked by anti-cheat systems.

### Building

#### Windows

Requires Visual Studio 2022 (or Build Tools) with the "Desktop development with C++" workload, which includes CMake. The Vulkan SDK is **not** required: shaders are compiled with `fxc.exe` from the Windows SDK (installed with the C++ workload).

```
cmake --preset windows-msvc
cmake --build --preset windows-msvc
ctest --preset windows-msvc
build\windows-msvc\cli\Release\freeframe-cli.exe gpus
```

#### Ubuntu

```
sudo apt install build-essential clang cmake ninja-build git
```

```
cmake --preset linux-clang
cmake --build --preset linux-clang
ctest --preset linux-clang
./build/linux-clang/cli/freeframe-cli gpus
```

You also need a Vulkan-capable driver: `mesa-vulkan-drivers` or the proprietary NVIDIA driver, plus `libvulkan1`.

### Usage (Windows, M1)

- Run the game in **windowed** or **borderless windowed** mode at a lower resolution (e.g. 1280x720). Exclusive fullscreen is not supported.
- Open `build\windows-msvc\app-windows\Release\FreeFrame.exe`. It shows no window; it waits for the hotkeys.
- With the game in focus, `Ctrl+Alt+S` toggles the scaling to fullscreen; `Ctrl+Alt+O` toggles the statistics (base FPS, output FPS, GPU ms).
- Command-line options: `--algo nearest|bilinear|fsr1` (default `fsr1`), `--sharpness 0..1` (default `0.5`), `--target-title "<text>"`, `--log <file>`, `--no-vsync`, `--present-every-vblank`, `--exit-after <s>`.
- Benchmark:

```
build\windows-msvc\cli\Release\freeframe-cli.exe bench-scale --algo all --src 1280x720 --dst 2560x1440
```

- Measured performance (RTX 4070, 1280x720 → 2560x1440, scaling stage only, GPU time):

| Algorithm | Time |
|---|---|
| Nearest | 0.08 ms |
| Bilinear | 0.04 ms |
| FSR 1 (EASU + RCAS) | 0.16 ms |

### Roadmap

- **M0** — skeleton ✅ done
- **M1** — overlay + FSR 1 on Windows 🚧 in progress
- **M2** — background detection + per-game profiles
- **M2.5** — all upscalers
- **M3** — frame generation X2
- **M4** — frame generation X3 / X4 / adaptive
- **M4.5** — statistics
- **M5** — Vulkan layer on Linux
- **M6** — extra backends
- **M7** — 1.0 release

### License

MIT. See [LICENSE](LICENSE) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

### Disclaimer

FreeFrame is an independent project, not affiliated with or derived from Lossless Scaling, Magpie, AMD, NVIDIA or Qualcomm.

---

## Português

Upscaling e geração de quadros gratuitos e de código aberto para qualquer jogo ou janela — Windows e Linux.

![CI](https://github.com/Trincamen/freeframe/actions/workflows/ci.yml/badge.svg)

**Status:** 🚧 Em desenvolvimento inicial — marco M1 (upscaling em overlay no Windows)

### Recursos (planejados)

- Upscaling espacial: FSR 1, NIS, SGSR, CAS, Integer, Nearest, Bilinear, Anime4K, xBR
- Geração de quadros X2 / X3 / X4 e adaptativa, sem suporte do jogo
- **Detecção de jogos em segundo plano com perfis automáticos por jogo** (o diferencial)
- Modo overlay (Windows, Windows Graphics Capture) e modo camada Vulkan (Linux, Proton)
- Estatísticas por jogo (antes / depois)
- 100% portátil

### Como funciona

- **Modo A — Overlay:** captura a janela alvo e exibe o resultado em uma janela própria.
- **Modo B — Camada Vulkan:** uma camada Vulkan que processa os quadros dentro do processo do jogo (Linux).

### Limitações

- A geração de quadros adiciona cerca de um quadro base de latência. Não é indicada para jogos competitivos.
- Recomenda-se um FPS base de pelo menos 40–60 para a geração de quadros.
- Tela cheia exclusiva não é suportada no modo overlay.
- Anti-cheat: o modo overlay não injeta nada no jogo. A camada Vulkan pode ser bloqueada por sistemas anti-cheat.

### Compilação

#### Windows

Requer Visual Studio 2022 (ou Build Tools) com a carga de trabalho "Desktop development with C++", que inclui o CMake. O Vulkan SDK **não** é necessário: os shaders são compilados com o `fxc.exe` do Windows SDK (instalado junto com o workload de C++).

```
cmake --preset windows-msvc
cmake --build --preset windows-msvc
ctest --preset windows-msvc
build\windows-msvc\cli\Release\freeframe-cli.exe gpus
```

#### Ubuntu

```
sudo apt install build-essential clang cmake ninja-build git
```

```
cmake --preset linux-clang
cmake --build --preset linux-clang
ctest --preset linux-clang
./build/linux-clang/cli/freeframe-cli gpus
```

Também é preciso um driver com suporte a Vulkan: `mesa-vulkan-drivers` ou o driver proprietário da NVIDIA, além de `libvulkan1`.

### Uso (Windows, M1)

- Rode o jogo em **janela** ou **janela sem borda** numa resolução menor (ex.: 1280x720). Tela cheia exclusiva não é suportada.
- Abra `build\windows-msvc\app-windows\Release\FreeFrame.exe`. Ele não mostra janela; fica esperando os atalhos.
- Com o jogo em foco, `Ctrl+Alt+S` liga/desliga a escala para a tela cheia; `Ctrl+Alt+O` liga/desliga as estatísticas (FPS base, FPS de saída, ms de GPU).
- Opções de linha de comando: `--algo nearest|bilinear|fsr1` (padrão `fsr1`), `--sharpness 0..1` (padrão `0.5`), `--target-title "<texto>"`, `--log <arquivo>`, `--no-vsync`, `--present-every-vblank`, `--exit-after <s>`.
- Benchmark:

```
build\windows-msvc\cli\Release\freeframe-cli.exe bench-scale --algo all --src 1280x720 --dst 2560x1440
```

- Desempenho medido (RTX 4070, 1280x720 → 2560x1440, só a etapa de escala, tempo de GPU):

| Algoritmo | Tempo |
|---|---|
| Nearest | 0.08 ms |
| Bilinear | 0.04 ms |
| FSR 1 (EASU + RCAS) | 0.16 ms |

### Roadmap

- **M0** — esqueleto ✅ concluído
- **M1** — overlay + FSR 1 no Windows 🚧 em andamento
- **M2** — segundo plano + detecção + perfis
- **M2.5** — todos os escaladores
- **M3** — geração de quadros X2
- **M4** — geração de quadros X3 / X4 / adaptativa
- **M4.5** — estatísticas
- **M5** — camada Vulkan no Linux
- **M6** — backends extras
- **M7** — lançamento 1.0

### Licença

MIT. Veja [LICENSE](LICENSE) e [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

### Aviso legal

O FreeFrame é um projeto independente, não afiliado nem derivado de Lossless Scaling, Magpie, AMD, NVIDIA ou Qualcomm.
