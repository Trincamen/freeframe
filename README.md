# FreeFrame

[English](#english) | [Português](#português)

---

## English

Free, open-source upscaling and frame generation for any game or window — Windows & Linux.

![CI](https://github.com/Trincamen/freeframe/actions/workflows/ci.yml/badge.svg)

**Status:** 🚧 Early development — milestone M0 (skeleton)

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

Requires Visual Studio 2022 (or Build Tools) with the "Desktop development with C++" workload, which includes CMake. The Vulkan SDK is **not** required for M0, since the loader comes with the GPU driver.

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

### Roadmap

- **M0** — skeleton
- **M1** — overlay + FSR 1 on Windows
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

**Status:** 🚧 Em desenvolvimento inicial — marco M0 (esqueleto)

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

Requer Visual Studio 2022 (ou Build Tools) com a carga de trabalho "Desktop development with C++", que inclui o CMake. O Vulkan SDK **não** é necessário para o M0, pois o loader vem com o driver da GPU.

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

### Roadmap

- **M0** — esqueleto
- **M1** — overlay + FSR 1 no Windows
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
