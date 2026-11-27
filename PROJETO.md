# PROJETO: FreeFrame (nome provisório) — upscaling + geração de quadros, grátis e open source

> **Para o Claude Code:** este arquivo é o briefing do projeto. Leia inteiro antes de começar.
> **Você (Opus) é o orquestrador:** siga a seção 0. Delegue para Sonnet e Haiku (máx. 2 e 4 ao
> mesmo tempo) e só programe pessoalmente quando a delegação falhar.
> Trabalhe **por marcos** (seção 9), um de cada vez. Ao fim de cada marco: o projeto compila
> em Windows e Linux, os testes passam e o README é atualizado. Como você não roda o programa na
> GPU do usuário, peça a ele que execute e devolva os logs, prints e números do benchmark.
> Sempre que uma decisão de arquitetura mudar, atualize este arquivo (e o registro na seção 14).

---

## 0. Como o trabalho é organizado: Opus orquestra, Sonnet e Haiku executam

O usuário inicia a sessão com o **Opus**. O Opus é **o líder do projeto**: planeja, divide,
delega, revisa e integra. **A prioridade é o Opus NÃO escrever o código ele mesmo.** Ele só
assume uma tarefa quando os subagentes não conseguem resolver (regra de escalonamento, 0.4).

### 0.1 Limite de paralelismo (obrigatório)
- No máximo **2 subagentes Sonnet** e **4 subagentes Haiku** rodando **ao mesmo tempo**
  (total de 6). Uma nova tarefa só é aberta quando uma vaga do mesmo tipo libera.
- Tarefas independentes devem ser lançadas em paralelo, dentro desse limite. Tarefas que dependem
  de outras esperam o resultado.
- Sonnets que editam código em paralelo trabalham em **worktrees isoladas** (`isolation: worktree`)
  ou em **arquivos/pastas diferentes** definidos pelo Opus. Dois agentes nunca editam o mesmo
  arquivo ao mesmo tempo.

### 0.2 Quem faz o quê
| Papel | Responsabilidades |
|---|---|
| **Opus (orquestrador)** | Ler este briefing, quebrar cada marco em tarefas pequenas, escolher o modelo de cada tarefa, definir quais arquivos cada agente pode tocar, revisar as entregas, integrar (merge), decidir a arquitetura, falar com o usuário e manter este arquivo e o `docs/PROGRESSO.md` atualizados |
| **Sonnet (até 2)** | Código difícil: núcleo Vulkan/D3D11, interop, captura WGC/DDA, camada Vulkan, optical flow e interpolação, shaders de upscaling, frame pacing, detecção de jogos, depuração de bugs reais, revisão de código dos Haikus |
| **Haiku (até 4)** | Tarefas rápidas e bem delimitadas: procurar no código, ler e resumir documentação/SDKs, conferir licenças, boilerplate (CMake, CI, manifestos JSON), presets e perfis JSON, lista de exclusão de processos, esqueleto de testes, strings de i18n e tradução PT/EN, README e docs, rodar build/lint/testes e **resumir os logs** |

Na dúvida entre Haiku e Sonnet: se a tarefa exige raciocínio sobre GPU, concorrência ou
sincronização, vai para o **Sonnet**.

### 0.3 Como o Opus delega (formato de cada tarefa)
Cada pedido a um subagente deve conter:
1. **Objetivo** em 1–2 frases e o marco a que pertence (ex.: "M1").
2. **Arquivos/pastas permitidos** (e os proibidos).
3. **Contexto mínimo** necessário (trechos deste briefing, interfaces já definidas). Não mandar o
   projeto inteiro.
4. **Critério de pronto** verificável (compila, teste X passa, saída esperada).
5. **Formato da resposta:** o que mudou (lista de arquivos), como testar, dúvidas e riscos.
   Resposta curta, sem colar arquivos inteiros.

### 0.4 Escalonamento quando algo falha
1. **Haiku falhou** (não compilou, entregou errado ou travou) → o Opus refaz o pedido com mais
   contexto **uma vez**. Se falhar de novo → passa a tarefa para um **Sonnet**.
2. **Sonnet falhou duas vezes** na mesma tarefa → o Opus tenta dividir a tarefa em partes menores
   e redelegar.
3. Só se ainda assim não houver solução, **o Opus assume a tarefa pessoalmente**, resolve, registra
   o motivo em `docs/PROGRESSO.md` e volta ao papel de orquestrador.
4. Bloqueios que dependem do usuário (testar na RTX 4070, rodar um jogo, instalar SDK) → o Opus
   para, explica de forma objetiva o que o usuário precisa fazer e espera.

### 0.5 Revisão e qualidade
- Todo código de Haiku é revisado por um Sonnet ou pelo Opus antes do merge. O código do Sonnet é
  revisado pelo Opus.
- Antes de encerrar um marco, um **Haiku** roda build + testes nos presets disponíveis e resume os
  resultados. O Opus confere o critério de aceite do marco (seção 9).
- O Opus mantém `docs/PROGRESSO.md` com: marco atual, tarefas (agente, status), decisões tomadas,
  pendências do usuário.

### 0.6 Definições de subagentes
Ficam em `.claude/agents/dev-sonnet.md` e `.claude/agents/dev-haiku.md` (criados no M0).

---

## 1. Visão

Um aplicativo **gratuito e open source**, para **Windows e Linux**, que entregue as principais
funções do *Lossless Scaling* (programa pago de US$ 6,99 na Steam, só para Windows):

1. **Upscaling espacial** de qualquer jogo ou janela: o jogo roda em janela com resolução
   menor e o programa amplia para a tela inteira com bons algoritmos.
2. **Geração de quadros (frame generation)** sem suporte do jogo: o programa cria quadros
   intermediários a partir da imagem capturada e multiplica o FPS percebido (2x, 3x, 4x, adaptativo).

**Diferencial em relação ao Lossless Scaling:** o LS precisa ser aberto e ativado manualmente
(botão "Scale" ou atalho) a cada jogo. O FreeFrame **fica rodando em segundo plano**, detecta
sozinho quando um jogo abre e ativa automaticamente o **perfil daquele jogo**. Se o jogo não for
reconhecido, o usuário pode adicioná-lo manualmente (seção 7A).

O objetivo também é ser um **projeto de portfólio** no GitHub: código limpo, CI, releases,
documentação e benchmarks reproduzíveis.

### Princípios (não negociáveis)
- **Nada de código, DLL, shader ou modelo do Lossless Scaling.** Não fazer engenharia reversa
  dele e não usar o nome nem a marca. (O projeto lsfg-vk depende da `Lossless.dll` paga. Nós não.)
- Usar apenas dependências com licença compatível (MIT, BSD, Apache, Zlib). As licenças estão
  na tabela da seção 6.
- Tudo o que for proprietário (por exemplo, o NVIDIA Optical Flow SDK) entra só como **plugin
  opcional, carregado em tempo de execução**, e nunca vai embutido no repositório.
- Funcionar em **qualquer GPU** com Vulkan 1.2+ ou D3D11. Recursos exclusivos de um fabricante
  ficam apenas como aceleração opcional.

### Hardware de desenvolvimento
- PC principal: **Windows + NVIDIA RTX 4070** (Ada, com acelerador de optical flow em hardware).
- PC de casa: **Ubuntu** (testes no Linux; interesse futuro em uma AMD RX 7600).

---

## 2. O que replicar do Lossless Scaling (referência funcional)

| Função do LS | O que fazemos | Prioridade |
|---|---|---|
| Ampliar um jogo em janela para tela cheia (modo "Scale" + atalho) | Captura da janela + overlay em tela cheia | **P0** |
| Escaladores: LS1 (proprietário), FSR 1, NIS, SGSR, Integer, Nearest, xBR, Anime4K, Sharp Bilinear, Bicubic CAS | Todos, menos o LS1 (licenças na seção 6). No lugar do LS1, um escalador próprio no futuro | P0/P1 |
| Nitidez ajustável | Slider de sharpness (RCAS/CAS/NIS) | P0 |
| Frame Gen modo **Fixo** (X2, X3, X4… o LS3 vai até X20) | Multiplicador fixo, começando por X2 | **P0** |
| Frame Gen modo **Adaptativo** (atinge um FPS alvo sem travar o FPS do jogo) | Multiplicador fracionário dinâmico | P1 |
| **Flow scale** (resolução do cálculo de movimento: menor = mais leve, maior = mais preciso) | Slider de 25% a 100% | P0 |
| **Performance mode** (mais leve, porém com mais artefatos) | Preset "Rápido" com menos níveis de pirâmide | P1 |
| Opções de latência (Queue target, Max frame latency) | Mesmas opções no menu avançado | P1 |
| Métodos de captura (DXGI / WGC) | WGC (padrão) + Desktop Duplication | P0/P1 |
| **Dual GPU** (uma GPU roda o jogo e outra faz o frame gen, com o monitor ligado nela) | Escolha da GPU de processamento | P2 |
| Contador de FPS (base e gerado) | Overlay com FPS base e de saída, além dos tempos de cada etapa | P0 |
| Perfis por jogo (no LS é manual) | **Perfil por jogo + detecção automática em segundo plano** (seção 7A) | **P0** |
| — (não existe no LS) | Adicionar jogo manualmente quando não for detectado | **P0** |
| Clip cursor, escala do cursor, ajustes de HDR | Igual | P2 |

Guias da comunidade de referência (para entender como os usuários configuram o LS): flow scale de
mais ou menos 50% em 1080p e 75–100% em 1440p/4K; modo fixo exige limitar o FPS do jogo a uma
fração da taxa de atualização; FPS base abaixo de ~30 gera muitos artefatos; o frame gen aumenta
a latência (não serve para jogos competitivos).

---

## 3. Arquitetura geral

Vamos ter **dois modos de operação** sobre um **núcleo comum**:

```
                ┌─────────────────────────── NÚCLEO (libfreeframe) ───────────────────────────┐
 Fonte de       │  Detecção de quadro novo → Optical Flow → Interpolação → Upscale → Nitidez   │   Saída
 quadros   ───► │  (descarta duplicados)     (pirâmide)     (warp bidirec.)  (FSR/NIS/…) (RCAS)  │ ──► Apresentação
                │  + Frame pacing / agendador de apresentação + métricas                         │
                └───────────────────────────────────────────────────────────────────────────────┘
 Modo A: OVERLAY  — captura a janela do jogo (WGC/DDA no Windows; PipeWire no Linux) e apresenta numa
                    janela própria, sem borda, sempre no topo e que deixa o clique passar (igual LS/Magpie).
 Modo B: LAYER    — camada Vulkan implícita que intercepta vkQueuePresentKHR e insere quadros
                    na própria swapchain do jogo (igual lsfg-vk). Linux primeiro (Proton/DXVK = Vulkan).
```

### 3.1 Núcleo de GPU (revisado — ver seção 14, decisões D2 e D3)
- **Interface `IGpuBackend`** desde o início, com dois backends:
  - **D3D11 (compute)** — backend **principal do Modo A no Windows**. A WGC já entrega textura
    D3D11, então não há interop no caminho crítico do M1.
  - **Vulkan 1.2+ (compute)** — backend do **Modo B (camada)**, do Linux e alternativo no Windows.
    Carregado via **volk** (o `vulkan-1.dll`/`libvulkan.so.1` do driver é aberto em tempo de
    execução; não precisa do Vulkan SDK para compilar o núcleo).
- **Shaders em HLSL, fonte única.** Compilados no build com **DXC**: HLSL → DXBC/DXIL (D3D11) e
  HLSL → SPIR-V (Vulkan). FSR1/CAS/NIS/SGSR já existem em HLSL. Shaders que só existem em GLSL
  (ex.: Anime4K) são portados para HLSL uma vez. O binário compilado é embutido no executável.
- O interop D3D11↔Vulkan (`VK_KHR_external_memory_win32` + semáforo compartilhado) deixa de ser
  obrigatório e vira opção futura, caso algum backend (ex.: FidelityFX OF em Vulkan) precise dele.
- Formatos: RGBA8/BGRA8 para SDR e RGBA16F para HDR (P2).

### 3.2 Modo A — Overlay (Windows primeiro)
- **Captura:**
  - **Windows.Graphics.Capture (WGC)** como padrão. É o que o Magpie recomenda pela melhor
    compatibilidade e fluidez. Captura uma janela específica (Windows 10 1903+). No Windows 11,
    desligar a borda amarela com `IsBorderRequired = false`.
  - **DXGI Desktop Duplication** como alternativa: monitor inteiro, menor latência, suporta HDR.
  - Jogos em **tela cheia exclusiva não são suportados**. O usuário deve usar janela ou janela sem borda.
- **Janela de saída:** sem borda, sempre no topo, transparente para cliques
  (`WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE`), excluída da própria captura
  (`SetWindowDisplayAffinity(WDA_EXCLUDEFROMCAPTURE)`), com swapchain flip model
  (`DXGI_SWAP_EFFECT_FLIP_DISCARD`) e tearing opcional.
- **Cursor:** esconder o cursor real sobre o jogo e desenhar o cursor escalado na saída (P1).
- **Linux:** captura via **xdg-desktop-portal + PipeWire** (Wayland) ou XComposite (X11). Fica
  para depois, porque no Linux o Modo B é mais natural.

### 3.3 Modo B — Camada Vulkan (Linux primeiro)
- Camada **implícita** (manifesto JSON), ativada por variável de ambiente, por exemplo
  `ENABLE_FREEFRAME=1 %command%` nas opções da Steam.
- Intercepta `vkCreateSwapchainKHR`, `vkQueuePresentKHR` e `vkAcquireNextImageKHR`. Copia a
  imagem apresentada, gera N quadros intermediários, adquire imagens extras da swapchain e chama o
  present do driver N+1 vezes, com sincronização por semáforos.
- Referência de estudo: a wiki do lsfg-vk ("Injecting frames into Vulkan apps") e o obs-vkcapture.
  **Estudar a técnica, não copiar código** (o lsfg-vk mudou para uma licença restritiva).
- Funciona com jogos Vulkan nativos e com DX9/10/11/12 via Proton (DXVK/VKD3D). OpenGL só via Zink.
- Na v1, avisar sobre incompatibilidade com outras camadas (MangoHud, vkBasalt) e desativar VRR.
- **No Windows** a mesma camada funciona com jogos Vulkan, mas pode acionar anti-cheat. Deixar
  como recurso experimental.

---

## 4. Geração de quadros — detalhes

### 4.1 Pipeline (por quadro real recebido)
1. **Detectar quadro novo:** comparar com o anterior (diferença média de luma, reduzida). A captura
   pode repetir quadros, e só entram quadros reais.
2. **Pirâmide** de luma em resolução reduzida (controlada pelo *flow scale*).
3. **Optical flow** bidirecional (anterior→atual e atual→anterior), do nível grosso ao fino.
4. **Interpolação** no tempo `t` (t = 1/2 para X2; 1/3 e 2/3 para X3…): warp dos dois quadros
   pelos vetores escalados por `t`, mistura ponderada, tratamento de oclusão (consistência
   forward/backward) e preenchimento de buracos.
5. **Pós-processamento:** upscale + nitidez, aplicados aos quadros reais e aos gerados.
6. **Agendamento:** apresentar os quadros em intervalos iguais (frame pacing), usando o tempo
   medido entre quadros reais.

> Observação: interpolação exige o **próximo** quadro real, então o atraso mínimo é de cerca de
> 1 quadro base. Isso é inerente à técnica (vale para LS, DLSS-FG e FSR-FG). Documentar no README.

### 4.2 Backends de optical flow (plugáveis, atrás de `IFlowEstimator`)
| Backend | Plataformas | Licença | Papel |
|---|---|---|---|
| **Próprio (compute shader)**: block matching hierárquico → refinamento estilo DIS | Todas | Nossa | **Padrão**, universal |
| **AMD FidelityFX Optical Flow + Frame Interpolation** (FSR 3.x) | DX12/Vulkan, qualquer GPU | MIT | Segundo backend; estudar e adaptar. Atenção: o FI da AMD foi feito para receber vetores de movimento e profundidade do jogo. Usar apenas o optical flow sobre a cor |
| **NVIDIA Optical Flow SDK (NVOFA) + FRUC** | RTX/Turing+ (a 4070 tem) | Proprietária (gratuita, exige conta de dev) | **Plugin opcional** carregado em tempo de execução; nunca no repositório |
| **RIFE via ncnn-vulkan** | Todas | MIT | Experimental, só para pesquisa. Medir se é viável em tempo real |

### 4.3 Parâmetros expostos
- Modo: Desligado / Fixo (X2…X8; limite configurável) / Adaptativo (FPS alvo).
- Flow scale (25–100%), Qualidade (Rápido / Equilibrado / Qualidade).
- Max frame latency (1–4), Queue target (0–2).
- Detecção de corte de cena (zera o histórico quando a diferença é muito grande).
- Opção de não interpolar o HUD (P2; difícil só com a cor da imagem).

---

## 5. Upscaling e filtros

Ordem do pipeline: **upscale → nitidez → (opcional) filtros extras**. Implementar como uma cadeia
de efeitos configurável, ao estilo dos "efeitos" do Magpie.

| Algoritmo | Uso recomendado | Fonte/licença |
|---|---|---|
| Nearest / Integer scaling | Pixel art, retrô | Próprio |
| Bilinear / Bicubic / Sharp Bilinear | Geral | Próprio |
| **FSR 1 (EASU + RCAS)** | Jogos 3D modernos | AMD FidelityFX — MIT |
| **CAS** (para o "Bicubic CAS") | Nitidez | AMD FidelityFX — MIT |
| **NIS** (NVIDIA Image Scaling) | Jogos 3D | NVIDIA, shaders HLSL/GLSL — MIT |
| **SGSR 1** (Snapdragon GSR) | Rápido, uma passada | Qualcomm — BSD-3 |
| **Anime4K** | Anime e 2D | bloc97 — MIT (GLSL estilo mpv; portar para HLSL) |
| **xBR** | Pixel art | ⚠️ Grande parte dos shaders xBR do libretro é **GPL** ou sem licença declarada. Usar só versão com licença MIT/BSD/domínio público verificada **ou implementação própria** a partir do algoritmo publicado (ver D5) |
| Escalador próprio (futuro, substituto do LS1) | 3D | Pequena rede convolucional em compute; marco de pesquisa |

---

## 6. Licença do projeto e dependências
- **Licença: MIT.** Atrai contribuições e é compatível com FidelityFX, NIS, SGSR, Anime4K,
  ncnn e RIFE.
  - ⚠️ O **Magpie é GPL-3.0**. Se copiarmos código dele, o projeto inteiro vira GPL-3.0. Decisão:
    **não copiar**, apenas usar como referência de arquitetura.
- **Gerência de dependências: CMake FetchContent** (sem vcpkg por enquanto; ver D6). Versões
  fixadas por tag/commit.
- Dependências: **Vulkan-Headers + volk** (Apache-2.0/MIT), VMA (MIT, quando houver alocação),
  **Dear ImGui** (MIT) para a UI, **ImPlot** (MIT) para os gráficos, **SQLite** (domínio público)
  para as estatísticas, **PresentMon** (MIT, opcional, para medição no Windows), **SDL3** ou GLFW
  (Zlib) para janelas no Linux, nlohmann/json (MIT), spdlog (MIT), **doctest** (MIT, testes),
  C++/WinRT (WGC no Windows), **DXC** (compilador de shaders, só no build; não é redistribuído).
- Arquivo `THIRD_PARTY_NOTICES.md` com todas as licenças incluídas.

---

## 7. Interface e experiência de uso
- **App principal** com bandeja do sistema: escolher a janela/jogo, preset, botão **Escalar** e
  atalho global (padrão `Ctrl+Alt+S`, configurável), com contagem regressiva opcional igual à do LS.
- **Perfis** por jogo, aplicados automaticamente (detalhes na seção 7A).
- **Overlay de estatísticas** (`Ctrl+Alt+O`): FPS base, FPS de saída, multiplicador atual, tempos
  da GPU (captura, flow, interpolação, upscale) e latência estimada.
- **Benchmark integrado** (`freeframe-cli benchmark`): roda o pipeline com quadros sintéticos ou
  gravados e mostra ms por etapa. É ótimo para o README e para comparar GPUs.
- **Linux:** CLI + arquivo de configuração para o Modo B, e uma UI simples (ImGui) que edita o arquivo.
- Idiomas: inglês + português (i18n desde o início, com strings fora do código).

---

## 7A. Modo em segundo plano, detecção de jogos e perfis (função principal)

### 7A.1 Serviço em segundo plano
- Processo leve na **bandeja do sistema** (meta: < 1% de CPU e < 50 MB de RAM enquanto espera).
- Inicia junto com o sistema (opcional; **desligado por padrão na versão portátil**, ver seção 8A):
  - **Windows:** tarefa agendada no logon com privilégios elevados (ver seção 8A.2b), removível
    pelo app.
  - **Linux:** arquivo `.desktop` em `~/.config/autostart` ou serviço `systemd --user`.
- Enquanto nenhum jogo está aberto, **não usa a GPU**. Os recursos de GPU só são criados na ativação.

### 7A.2 Como detectar que um jogo abriu
**Windows**
1. **Novo processo:** varrer a lista de processos a cada 1–2 s (`EnumProcesses`/Toolhelp32;
   simples e sem admin). Opcionalmente usar eventos ETW/WMI (`Win32_ProcessStartTrace`, exige
   admin) para resposta instantânea.
2. **Janela em foco:** `SetWinEventHook(EVENT_SYSTEM_FOREGROUND)` para saber quando o jogo vem
   para frente e quando sai (Alt+Tab).
3. **É um jogo?** A identificação segue esta ordem:
   - **a)** Existe um perfil para o executável ou para o caminho → é jogo.
   - **b)** O executável está numa **biblioteca de jogos conhecida**:
     - Steam: `steamapps/common`, lido de `libraryfolders.vdf` e dos `appmanifest_*.acf`, que dão o nome e o AppID.
     - Epic: `C:\ProgramData\Epic\EpicGamesLauncher\Data\Manifests\*.item`.
     - GOG: registro `HKLM\SOFTWARE\WOW6432Node\GOG.com\Games`.
     - Xbox/Game Pass: pasta `XboxGames`.
     - EA, Ubisoft e Battle.net (P2).
   - **c)** **Heurística**: o processo carregou `d3d11.dll`, `d3d12.dll`, `vulkan-1.dll` ou
     `opengl32.dll` e tem uma janela grande, sem borda ou em tela cheia. Nesse caso é tratado
     como **"possível jogo"** e a ativação só ocorre se o usuário confirmar (notificação).
     Inspecionar módulos exige `OpenProcess`; no **modo limitado** (sem admin) isso falha em parte
     dos processos, e a heurística deve **degradar em silêncio** (usar só tamanho/estilo da janela).
   - **d)** Uma **lista de exclusão** (launchers, navegadores, Discord, OBS, editores, o próprio
     FreeFrame) nunca é tratada como jogo.

**Linux**
- **Modo B (camada Vulkan):** a camada já é carregada dentro do jogo. Ela lê o nome do executável
  (`/proc/self/exe`) e a variável `SteamAppId`, procura o perfil e se ativa ou fica desligada. O
  "segundo plano" no Linux é basicamente isso, mais o app de bandeja que edita os perfis e mostra o
  status. Para não ficar sempre ativa, a camada pode ser instalada como implícita e controlada por
  perfil, além da variável `ENABLE_FREEFRAME`.
- **Modo A no Linux** (futuro): varrer `/proc` e pegar a janela em foco (X11/Wayland, quando possível).

### 7A.3 O que acontece ao detectar
Comportamento configurável por perfil e com um padrão global:
- **Ativar automaticamente**: espera X segundos (padrão 5 s, para pular telas de carregamento e
  launchers), confirma que a janela está em modo janela/sem borda e ativa.
- **Perguntar**: mostra uma notificação "Jogo X detectado — ativar? [Sim] [Sempre] [Nunca]".
- **Não fazer nada**: o jogo fica conhecido, mas desativado.
- Se o jogo estiver em **tela cheia exclusiva** (o Modo A não consegue capturar), avisar e sugerir
  "Janela sem borda".
- **Ao perder o foco** (Alt+Tab): pausar o processamento. Ao voltar: retomar. Ao fechar o jogo:
  liberar a GPU e voltar a esperar.
- Atalho global sempre disponível para **ligar/desligar na hora** (`Ctrl+Alt+S`).

### 7A.4 Adicionar jogo manualmente (quando não for detectado)
Na UI, botão **"Adicionar jogo"** com três opções:
1. **Escolher entre as janelas abertas agora** (lista com ícone e título). É o mais fácil.
2. **Procurar o executável** (`.exe` no Windows; binário ou AppID da Steam no Linux).
3. **"Capturar a próxima janela em foco"**: o usuário clica no botão, vai para o jogo e o
   FreeFrame registra a janela que ganhou o foco.

Também dá para **marcar como "não é jogo"** qualquer coisa detectada por engano.

### 7A.5 Perfis por jogo
- Um arquivo por jogo em `data/perfis/`, ao lado do executável (modo portátil, padrão). No modo
  instalado: `%APPDATA%\FreeFrame\perfis\` ou `~/.config/freeframe/perfis/`. Nome do arquivo = id do jogo.
- **Perfil "Padrão"** global: todo jogo novo herda dele, e o perfil do jogo guarda só o que difere.
- Campos sugeridos:
```json
{
  "id": "steam-1091500",
  "nome": "Cyberpunk 2077",
  "identificacao": { "exe": "Cyberpunk2077.exe", "caminho_contem": "steamapps/common/Cyberpunk 2077", "steam_appid": 1091500 },
  "ao_detectar": "ativar",            // ativar | perguntar | nada
  "atraso_ativacao_s": 5,
  "modo": "overlay",                  // overlay | layer
  "captura": "wgc",                   // wgc | dda
  "upscale": { "algoritmo": "fsr1", "nitidez": 0.5, "resolucao_saida": "nativa" },
  "frame_gen": { "modo": "fixo", "multiplicador": 2, "fps_alvo": 144, "flow_scale": 0.5, "qualidade": "equilibrado" },
  "latencia": { "max_frame_latency": 2, "queue_target": 1 },
  "overlay_fps": true,
  "notas": ""
}
```
- **Ações na UI:** criar, duplicar, renomear, exportar/importar (compartilhar perfil com amigos),
  restaurar o padrão, e **presets prontos** ("Jogo 3D leve", "Jogo 3D pesado", "Pixel art",
  "Anime/2D", "Emulador").
- **Validação:** `freeframe-cli validate` verifica todos os perfis. Campos desconhecidos geram aviso,
  sem quebrar o programa.
- **Futuro (P2):** perfis da comunidade (repositório JSON no GitHub com configurações recomendadas por jogo).

### 7A.6 Tela principal sugerida
- **Biblioteca:** lista de jogos detectados e adicionados, com ícone, nome, origem (Steam, Epic,
  Manual…), status (ativo, só perfil, ignorado) e botão de editar o perfil.
- **Status ao vivo:** jogo atual, perfil em uso, FPS base → FPS de saída.
- **Configurações gerais:** iniciar com o sistema, comportamento padrão ao detectar, atalhos,
  idioma, lista de exclusão.

---

## 7B. Tela de Estatísticas: "antes e depois" por jogo e ganho de cada tecnologia

Uma aba **Estatísticas** que mostra, para cada jogo, quanto o FreeFrame ajudou e **qual tecnologia
foi responsável por cada parte do ganho**. Tudo fica **salvo só no PC do usuário**, sem enviar
dados para fora.

### 7B.1 O que medir (por sessão de jogo)
- **FPS base** (quadros reais que o jogo produz) e **FPS exibido** (reais + gerados). Mostrar sempre
  os dois separados, porque quadro gerado não é igual a quadro real.
- **1% low / 0,1% low** e **frametime** (média, percentis 95/99, histograma), para medir a estabilidade.
- **Custo de cada etapa na GPU** (ms): captura, optical flow, interpolação, upscale e nitidez,
  medido com timestamps de GPU (seção 10).
- **Latência estimada** adicionada pelo frame gen.
- **Uso, temperatura e VRAM da GPU** (opcional): NVML na NVIDIA (RTX 4070) e sysfs/amdgpu no Linux.
- **Configuração usada** (perfil, algoritmo, multiplicador, flow scale, resoluções) para cada
  número ficar ligado ao que estava ligado.

### 7B.2 Como obter o "antes"
O "antes" é o jogo **sem o FreeFrame**. Três formas, da mais simples à mais precisa:
1. **Linha de base automática:** quando um jogo é detectado pela primeira vez, o FreeFrame mede
   ~20 s **sem processar nada** (só conta os quadros) e guarda como "antes". O usuário pode refazer
   a medição quando quiser.
2. **Teste comparativo (A/B):** botão "Comparar agora". Alterna desligado → só upscale → upscale +
   frame gen, em blocos de ~15–30 s na mesma cena, e gera o relatório. É o modo mais justo.
3. **Medição externa sem injeção:** no Windows, usar o **PresentMon** (Intel, open source, MIT,
   via ETW, sem tocar no jogo) para medir o FPS e o frametime real do processo do jogo, com ou sem
   o FreeFrame. No Linux, a própria camada Vulkan conta os presents (ou importar logs do MangoHud).

> Para medir o ganho do **upscaling** é preciso saber o FPS do jogo **na resolução nativa** (antes)
> e **na resolução reduzida** (depois). O teste A/B pede ao usuário para trocar a resolução do jogo
> e guia o processo passo a passo.

### 7B.3 Como mostrar a contribuição de cada tecnologia
- **Gráfico "cascata" (waterfall)** por jogo, por exemplo:
  `FPS nativo 1440p: 48` → `+ Upscale (jogo em 960p + FSR1): 72 (+24)` →
  `− custo do frame gen: 66 (−6)` → `× Frame gen X2: 132 exibidos`.
  Cada barra é uma tecnologia, com cor e legenda próprias.
- **Barras antes × depois:** FPS médio, 1% low e frametime, lado a lado.
- **Linha do tempo** da sessão: FPS base e exibido ao longo do jogo, com marcações de quando o
  usuário mudou alguma configuração.
- **Histograma de frametime** antes × depois (mostra se ficou mais estável).
- **Pizza ou barra empilhada do custo da GPU** por etapa (captura, flow, interpolação, upscale).
- **Card de latência:** "+X ms estimados com frame gen".
- **Visão geral (todos os jogos):** ranking dos jogos com maior ganho, tecnologia mais usada, horas
  jogadas com o FreeFrame ativo.
- **Comparação visual (P2):** captura de tela "antes/depois" com um divisor arrastável (lado a lado)
  para comparar a qualidade do upscaling.

### 7B.4 Armazenamento e exportação
- Banco local **SQLite** (domínio público), com tabelas `jogos`, `sessoes`, `amostras` (1 amostra por
  segundo) e `testes_ab`. Limpeza automática de amostras antigas, mantendo os resumos por sessão.
- **Exportar** para CSV/JSON e **gerar uma imagem (PNG) do relatório** para compartilhar. Isso é
  ótimo para o README e para divulgar o projeto.
- Opção para desligar a coleta de estatísticas.

### 7B.5 Tecnologia da tela
- Gráficos com **ImPlot** (MIT; extensão do Dear ImGui, que já é a UI do projeto). Suporta barras,
  linhas, histogramas e barras empilhadas.
- Os cálculos de estatística (percentis, 1% low, cascata) ficam no núcleo (`core/stats`), com
  testes unitários.

---

## 8. Estrutura do repositório e build

```
freeframe/
├─ CMakeLists.txt  CMakePresets.json  cmake/ (módulos, FetchContent das dependências)
├─ core/              # libfreeframe: backends de GPU, flow, interpolação, upscalers, pacing
│  ├─ include/  src/
│  └─ shaders/        # HLSL → DXBC/DXIL + SPIR-V via DXC (compilados no build, embutidos)
├─ app-windows/       # Modo A: WGC/DDA, janela overlay, bandeja, UI
├─ stats/             # coleta de métricas, SQLite, testes A/B, integração PresentMon/NVML, tela de gráficos (ImPlot)
├─ watcher/           # serviço em segundo plano: detecção de processos/janelas, bibliotecas (Steam/Epic/GOG), perfis
├─ app-linux/         # Modo A no Linux (PipeWire) — depois
├─ layer-vulkan/      # Modo B: camada implícita + manifesto JSON
├─ cli/               # benchmark, validação de config, ferramentas de teste
├─ plugins/nvofa/     # plugin opcional NVIDIA (compilado só se o SDK estiver presente)
├─ tests/             # unitários + testes de imagem (golden images)
├─ tools/             # scripts: gravar sequências, medir PSNR/SSIM
├─ docs/              # arquitetura, guia de uso, FAQ, comparação de qualidade
└─ .github/workflows/ # CI: build Windows (MSVC) + Ubuntu (GCC/Clang), testes, release
```
- **C++20**, CMake 3.25+, presets `windows-msvc` e `linux-clang`.
- **CI:** compilar e testar nos dois sistemas a cada PR. Gerar release **portátil** com `.zip`
  (Windows) e **AppImage** + `.tar.xz` (Linux) quando houver tag `v*` (ver seção 8A).
- Formatação com clang-format e análise com clang-tidy; warnings como erro no CI.

---

## 8A. Distribuição 100% portátil (requisito)

O FreeFrame deve rodar **sem instalador**: o usuário baixa, extrai e abre. No Windows ele pede
permissão de administrador ao abrir (seção 8A.2b), mas não instala nada. Apagar a pasta remove o programa por completo (com uma exceção opcional, explicada em 8A.3).

### 8A.1 Windows: pasta `.zip`
```
FreeFrame/
├─ FreeFrame.exe          # app + bandeja + overlay + UI
├─ freeframe-cli.exe
├─ portable.txt           # se existir, todos os dados ficam em ./data (padrão no zip)
├─ data/                  # perfis/, estatisticas.db, config.json, logs/
├─ layer/                 # camada Vulkan (Modo B, opcional) + manifesto JSON
└─ plugins/               # pasta vazia; o usuário pode colocar plugins opcionais (ex.: NVIDIA FRUC)
```
- **Sem dependências para instalar:** compilar com o runtime do MSVC **estático** (`/MT`) e com
  shaders já compilados (SPIR-V/DXBC **embutidos no .exe**). Nada de .NET, Visual C++
  Redistributable ou Vulkan SDK na máquina do usuário.
- O que vem com o próprio Windows ou com o driver da GPU já basta: `vulkan-1.dll` (instalado pelo
  driver da NVIDIA/AMD/Intel), D3D11/DXGI, Windows.Graphics.Capture e `nvml.dll` (driver NVIDIA).
- **Atenção:** o `.exe` sem assinatura digital mostra o aviso do SmartScreen. Documentar no README
  ("Mais informações → Executar assim mesmo"). Assinatura de código fica como opcional para o futuro.

### 8A.2 Linux: **AppImage** (um arquivo só) + `.tar.xz`
- AppImage para o app de bandeja/UI.
- A camada Vulkan (Modo B) funciona **sem root**, de duas formas:
  - **Totalmente portátil:** variável nas opções de inicialização da Steam, apontando para a pasta
    do FreeFrame: `VK_ADD_LAYER_PATH=/caminho/FreeFrame/layer ENABLE_FREEFRAME=1 %command%`.
    O app gera essa linha pronta com o botão "Copiar".
  - **Automática:** registrar o manifesto em `~/.local/share/vulkan/implicit_layer.d/` (pasta do
    usuário, sem root), com um botão "Ativar/Desativar" no app.
- Steam instalada via Flatpak exige tratamento especial (o caminho precisa estar acessível dentro da
  sandbox). Documentar na FAQ.

### 8A.2b Executar como administrador no Windows (decisão do usuário; mecanismo revisado — D1)
Para evitar erros de permissão, o **FreeFrame.exe tenta rodar como administrador no Windows**:
- **Mecanismo:** o manifesto do executável usa `requestedExecutionLevel level="asInvoker"`. Ao
  abrir, se o processo não estiver elevado, ele se **relança com `ShellExecuteExW` verbo `runas`**
  (o Windows mostra o UAC) e a instância não elevada encerra.
  - Motivo: com `requireAdministrator` no manifesto, se o usuário recusar o UAC o processo nem
    chega a iniciar, então não teria como abrir em modo limitado.
  - Argumento `--no-elevate` para pular a tentativa (testes, CI, preferência do usuário).
  - O `freeframe-cli.exe` **não** pede elevação (é usado no CI e em scripts).
- Benefícios: o **PresentMon/ETW fica sempre disponível** (medição precisa nas Estatísticas), a
  detecção de processos enxerga tudo e é possível capturar jogos que também rodam elevados.
- **Iniciar com o Windows já elevado:** a pasta Inicializar não abre programas como admin sem
  mostrar o UAC. Por isso usar uma **tarefa agendada no logon com "Executar com privilégios mais
  altos"** (Agendador de Tarefas, criada pelo próprio app). Assim ele abre sozinho, sem pedir
  confirmação a cada boot.
- **Se o usuário recusar o UAC** (`ShellExecuteExW` retorna `ERROR_CANCELLED`): a instância
  original continua em **modo limitado**, com um aviso explicando o que fica indisponível
  (PresentMon, inspeção de módulos de alguns processos). Ela nunca fecha com erro.
- Cuidados de código para processo elevado:
  - Arrastar e soltar de janelas não elevadas é bloqueado pelo Windows (UIPI). Usar diálogos de
    "Procurar" em vez disso.
  - Nunca abrir links/navegador como admin: usar `ShellExecute` com o token do usuário normal (ou
    via explorer).
  - Validar caminhos e arquivos de perfil, porque um processo admin não pode carregar DLL de
    qualquer pasta. Plugins só de `plugins/`, com aviso na primeira vez.
- **Linux:** **não** rodar como root (quebra Vulkan, Wayland e as configurações do usuário). A camada
  Vulkan roda dentro do processo do jogo e não precisa de privilégios. Se for aberto com `sudo`,
  avisar e sair.

### 8A.3 O que exige mexer no sistema (sempre opcional, sempre reversível pelo app)
| Recurso | O que altera | Precisa de admin? | No modo portátil |
|---|---|---|---|
| Iniciar com o sistema | Tarefa agendada no logon com privilégios elevados (Win) / `~/.config/autostart` (Linux) | Win: sim (já elevado) | **Desligado por padrão**; o usuário liga se quiser |
| Camada Vulkan no Windows (Modo B) | Chave em `HKCU\Software\Khronos\Vulkan\ImplicitLayers` | Não | Desligado; botão "Ativar/Remover" |
| PresentMon (medição precisa, seção 7B.2) | Nada instalado; usa ETW (exige admin) | Sim (o app já roda elevado) | **Ligado** no Windows. No modo limitado, cai para a contagem própria |
| Plugin NVIDIA FRUC | Usuário copia a DLL para `plugins/` | Não | Opcional |

- Botão **"Remover tudo o que o FreeFrame alterou no sistema"** em Configurações, que desfaz cada
  item dessa tabela. Assim nada fica para trás ao apagar a pasta.
- Sem o `portable.txt`, os dados vão para `%APPDATA%\FreeFrame` / `~/.config/freeframe` (modo
  "instalado"), útil para quem prefere assim.

### 8A.4 Critério no CI
- O job de release gera o `.zip` e o AppImage e roda um teste: **iniciar numa VM/runner limpo,
  sem SDKs**, executar `freeframe-cli --version` e `freeframe-cli gpus` (no runner sem GPU, deve
  falhar de forma limpa, com mensagem clara).
- Conferir as dependências do `.exe` (`dumpbin /dependents`) e da AppImage: só bibliotecas do sistema
  ou do driver.

---

## 9. Roteiro por marcos (com critério de aceite)

**M0 — Esqueleto (1 sessão)**
Repositório, CMake, CI verde nos dois sistemas, README, licença, `IGpuBackend` com Vulkan
(via volk, sem precisar do SDK) inicializando e listando as GPUs; no Windows, também listar os
adaptadores DXGI. ✔ Aceite: `freeframe-cli gpus` lista a RTX 4070.

**M1 — Mini-Magpie no Windows**
Captura WGC de uma janela → janela overlay em tela cheia → upscale Nearest/Bilinear/**FSR1**,
atalho global, overlay de FPS. Backend **D3D11** e pipeline de shaders HLSL via DXC. ✔ Aceite: um
jogo em 1280x720 em janela aparece em tela cheia a 1080p/1440p, sem stutter visível e com
captura+escala abaixo de 2 ms na 4070.

**M2 — Segundo plano + detecção + perfis (o diferencial)**
App de bandeja, início com o sistema, detecção de processos e foco, leitura das bibliotecas
Steam/Epic/GOG, heurística + exclusões, "Adicionar jogo" (3 formas), perfis JSON com herança do
padrão e UI de biblioteca. ✔ Aceite: abrir um jogo da Steam ativa o FSR1 sozinho em até ~5 s;
um jogo fora das bibliotecas pode ser adicionado em 2 cliques; Alt+Tab pausa e retoma; fechar o
jogo libera a GPU; o consumo ocioso fica dentro da meta da seção 7A.1.

**M2.5 — Todos os escaladores**
NIS, SGSR, CAS, Sharp Bilinear, Integer, Anime4K, xBR (ver D5), slider de nitidez e presets de perfil.

**M3 — Frame generation X2 (backend próprio)**
Detecção de quadro novo, optical flow hierárquico, interpolação com oclusão, frame pacing, flow
scale. ✔ Aceite: com o jogo travado em 60 FPS, a saída fica em ~120 FPS estáveis; o custo do FG
na 4070 em 1440p é ≤ 3 ms com flow scale de 50%; PSNR medido pelo protocolo da seção 10.

**M4 — X3/X4 + modo Adaptativo + opções de latência**

**M4.5 — Tela de Estatísticas (seção 7B)**
Coleta por sessão, linha de base automática, teste A/B guiado, gráficos (cascata, antes × depois,
linha do tempo, histograma, custo da GPU), exportação CSV e PNG. ✔ Aceite: depois de jogar 10 min,
o jogo aparece com o gráfico cascata correto e os números batem com o PresentMon (diferença ≤ 5%).
(A coleta básica de FPS pode começar já no M1; a tela completa entra aqui.)

**M5 — Camada Vulkan no Linux (Modo B)**
Funciona com um jogo Vulkan nativo e com um jogo via Proton no Ubuntu. Instalação em `~/.local`.

**M6 — Backends extras**
FidelityFX Optical Flow; plugin NVOFA/FRUC opcional; comparação de qualidade e velocidade entre eles.

**M7 — Polimento para release 1.0**
Zip portátil, documentação, vídeo comparativo, página do projeto, i18n PT/EN. Conferir se o nome
"FreeFrame" está livre (ou escolher o nome definitivo).

**Futuro:** Dual GPU, HDR, captura PipeWire no Linux, escalador próprio com ML, máscara de HUD,
integração com VR (ver observação na seção 11).

---

## 10. Testes e medição de qualidade
- **Unitários (doctest):** matemática de pacing, parser de config, detecção de quadros duplicados.
- **Golden images:** cada escalador roda sobre imagens fixas e é comparado a referências
  (tolerância por PSNR).
- **Protocolo de qualidade do FG** (mostra objetividade no portfólio):
  gravar uma sequência real a 120 FPS → descartar os quadros ímpares → interpolar a partir dos
  pares → comparar com os ímpares originais (PSNR, SSIM e, se possível, LPIPS). Publicar a tabela no README.
- **Desempenho:** timestamps de GPU (`vkCmdWriteTimestamp` / queries D3D11) por etapa, exportados em CSV.
- **Latência:** documentar como medir (por exemplo, NVIDIA FrameView/PresentMon no Windows).

---

## 11. Riscos e limitações conhecidas
- **Latência:** interpolação sempre adiciona pelo menos ~1 quadro base. Avisar o usuário e
  recomendar FPS base ≥ 40–60.
- **Artefatos** em HUD, bordas de tela, objetos rápidos e transparências. São inevitáveis só com a
  cor da imagem; o objetivo é minimizar.
- **Anti-cheat:** o Modo A (captura) é seguro, porque não injeta nada no jogo. O Modo B (camada) e
  qualquer hook podem ser bloqueados. Nunca injetar DLL em jogos online.
- **Tela cheia exclusiva** e conteúdo protegido (DRM) não podem ser capturados.
- **VRR/G-Sync** com overlay: comportamento varia; documentar.
- **Interop D3D11↔Vulkan** no Windows: risco técnico, mitigado pela decisão D3 (Modo A usa D3D11
  nativo; interop só se algum backend futuro exigir).
- **VR (Meta Quest 3 / Steam Link):** fora do escopo da v1. Em VR, a suavização é feita pelo
  runtime (reprojection/motion smoothing do SteamVR). Como referência futura, existem projetos de
  frame gen para VR via camada OpenXR usando optical flow.

---

## 12. Projetos e documentos de referência (estudar, respeitando as licenças)
- Magpie (escalador de janelas no Windows, GPL-3.0; arquitetura e métodos de captura): https://github.com/Blinue/Magpie — wiki "Comparison of capture methods"
- lsfg-vk (camada Vulkan; ler a wiki "Injecting frames into Vulkan apps" para entender a técnica): https://github.com/PancakeTAS/lsfg-vk e lsfg-vk.dev
- AMD FidelityFX SDK (FSR1/RCAS/CAS, FSR3 Optical Flow + Frame Interpolation, MIT): https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK
- NVIDIA Image Scaling SDK (MIT): https://github.com/NVIDIAGameWorks/NVIDIAImageScaling
- Snapdragon GSR (BSD-3): https://github.com/SnapdragonStudios/snapdragon-gsr
- Anime4K (MIT): https://github.com/bloc97/Anime4K
- RIFE ncnn Vulkan (MIT): https://github.com/nihui/rife-ncnn-vulkan
- NVIDIA Optical Flow SDK 5.0 (NVOFA + FRUC; tem interface Vulkan em Ampere+): https://developer.nvidia.com/opticalflow-sdk
- obs-vkcapture (outro exemplo de camada Vulkan): https://github.com/nowrep/obs-vkcapture
- gamescope (compositor da Valve com FSR/NIS no Linux): https://github.com/ValveSoftware/gamescope
- volk (meta-loader Vulkan, MIT): https://github.com/zeux/volk
- DXC (DirectX Shader Compiler, HLSL → DXIL/SPIR-V): https://github.com/microsoft/DirectXShaderCompiler
- Projetos pequenos para comparar ideias (imaturos; não copiar sem conferir a licença): FrGen (openxit/FrGen), LFG (furkan-1337/LFG), OFXR-Bridge (frame gen para VR via OpenXR)
- Windows.Graphics.Capture: https://learn.microsoft.com/windows/apps/develop/media-authoring-processing/screen-capture
- Desktop Duplication API: https://learn.microsoft.com/windows/win32/direct3ddxgi/desktop-dup-api

---

## 13. Primeira tarefa para o Claude Code (concluída a etapa 1)
1. ~~Ler este arquivo e propor ajustes~~ — feito; ajustes aprovados pelo usuário (seção 14).
2. Criar os subagentes da seção 0.6 e o `docs/PROGRESSO.md`. Montar o plano do M0 dividido em
   tarefas, com o modelo de cada uma, e mostrar ao usuário.
3. Executar o **M0** delegando.
4. Explicar ao usuário como compilar e rodar no Windows (Visual Studio 2022) e no Ubuntu.
5. Esperar o resultado do usuário antes de seguir para o M1.

---

## 14. Registro de decisões de arquitetura
| # | Data | Decisão | Motivo |
|---|---|---|---|
| D1 | 2026-11-26 | Manifesto `asInvoker` + autoelevação via `ShellExecuteExW "runas"`; recusa do UAC → modo limitado. CLI nunca eleva | Com `requireAdministrator`, recusar o UAC impede o processo de iniciar, então o modo limitado seria impossível |
| D2 | 2026-11-26 | Shaders em **HLSL fonte única**, compilados com DXC para DXBC/DXIL e SPIR-V | FSR1/CAS/NIS/SGSR já são HLSL; torna o backend D3D11 barato |
| D3 | 2026-11-26 | Modo A no Windows usa **backend D3D11** nativo; Vulkan para Modo B/Linux e alternativo no Windows | WGC entrega D3D11; elimina o interop do caminho crítico do M1 |
| D4 | 2026-11-26 | Vulkan via **Vulkan-Headers + volk** (carregamento dinâmico) | Compila sem Vulkan SDK; CI simples; executável portátil |
| D5 | 2026-11-26 | xBR: só fonte com licença permissiva verificada ou implementação própria | Muitos shaders xBR do libretro são GPL |
| D6 | 2026-11-26 | Dependências via **CMake FetchContent** (sem vcpkg por ora); testes com **doctest** | Clonar e compilar sem passos extras; compatível com `/MT` |
