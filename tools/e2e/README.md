# FreeFrame e2e input/focus test

Reproduces the "overlay steals focus/input" class of bugs (seen with Silent Hill 2) without a human.

`tools/testgame` builds `freeframe-testgame.exe`: a windowed D3D11 app (1280x720 client, fixed test pattern,
moving square, frame counter) that logs focus (`ACTIVATE`, `FOCUS`), keyboard, mouse, XInput and a 1 s
`STAT fps=.. foreground=..` line. Options: `--size WxH --pos x,y --fps N --log FILE --pause-on-inactive --exit-after S`.

`input_test.ps1` starts the test game, starts `FreeFrame.exe --target-title "FreeFrame Test Game"`, injects
keys (A, Space, Enter, B), three clicks (2 left, 1 right) inside the client rect and a focus loss/return via
SendInput (no Alt), then checks: game stays foreground, every key/click reaches the game with in-client
coordinates, FreeFrame logs `Scaling 1280x720 -> ...`, and after FreeFrame exits the cursor clip is the whole
desktop and the cursor is visible. It always cleans up (kills processes, `ClipCursor(NULL)` if needed).

## Run

    cmake -S . -B build/e2e -G "Visual Studio 17 2022" -A x64
    cmake --build build/e2e --config Release --target freeframe-testgame
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\e2e\input_test.ps1 [-TestGame <exe>] [-FreeFrame <exe>]

Defaults: `build\e2e\tools\testgame\Release\freeframe-testgame.exe` and
`build\windows-msvc\app-windows\Release\FreeFrame.exe`. Exit code 0 = all PASS. The test takes over the
mouse/keyboard for ~12 s; logs are kept in `%TEMP%\ffe2e_*`.

## Limitations

- Gamepad: without a virtual driver (ViGEm) XInput cannot be simulated; the test game only logs a real pad
  (`XINPUT pad0 connected/buttons=0x....`). Test controllers manually.
- Games that read raw input / DirectInput / exclusive fullscreen are not modeled; only window messages.
