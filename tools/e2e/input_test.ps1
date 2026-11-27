<#
  FreeFrame e2e input/focus test (Windows PowerShell 5.1).
  Opens freeframe-testgame, starts FreeFrame on it, injects keyboard/mouse with SendInput and checks
  that the game keeps foreground and receives everything. Covers the cursor on exit as well.
  Exit code: 0 = all checks passed, 1 = at least one failed.
  NOTE: takes over keyboard/mouse for ~12 s. Do not use Alt in simulations (system menu).
#>
param(
    [string]$TestGame = '',
    [string]$FreeFrame = '',
    [int]$ExitAfter = 12
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if (-not $TestGame)  { $TestGame  = Join-Path $repo 'build\e2e\tools\testgame\Release\freeframe-testgame.exe' }
if (-not $FreeFrame) { $FreeFrame = Join-Path $repo 'build\windows-msvc\app-windows\Release\FreeFrame.exe' }
$TestGame  = [IO.Path]::GetFullPath($TestGame)
$FreeFrame = [IO.Path]::GetFullPath($FreeFrame)

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class W32 {
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct CURSORINFO { public int cbSize; public int flags; public IntPtr hCursor; public POINT pt; }
    [StructLayout(LayoutKind.Sequential)] public struct MOUSEINPUT { public int dx, dy; public uint mouseData, dwFlags, time; public IntPtr extra; }
    [StructLayout(LayoutKind.Sequential)] public struct KEYBDINPUT { public ushort wVk, wScan; public uint dwFlags, time; public IntPtr extra; }
    [StructLayout(LayoutKind.Explicit)] public struct IU { [FieldOffset(0)] public MOUSEINPUT mi; [FieldOffset(0)] public KEYBDINPUT ki; }
    [StructLayout(LayoutKind.Sequential)] public struct INPUT { public uint type; public IU u; }

    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr c);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern bool GetCursorPos(out POINT p);
    [DllImport("user32.dll")] public static extern uint SendInput(uint n, INPUT[] i, int size);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool GetClipCursor(out RECT r);
    [DllImport("user32.dll")] public static extern bool ClipCursor(IntPtr r);
    [DllImport("user32.dll")] public static extern bool GetCursorInfo(ref CURSORINFO ci);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern IntPtr GetShellWindow();
    [DllImport("user32.dll")] public static extern IntPtr FindWindow(string cls, string title);
    [DllImport("user32.dll")] public static extern int GetSystemMetrics(int i);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool attach);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
    [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();

    public static void Key(ushort vk, bool up) {
        var i = new INPUT[1]; i[0].type = 1; i[0].u.ki.wVk = vk; i[0].u.ki.dwFlags = up ? 2u : 0u;
        SendInput(1, i, Marshal.SizeOf(typeof(INPUT)));
    }
    public static void Mouse(uint flags) {
        var i = new INPUT[1]; i[0].type = 0; i[0].u.mi.dwFlags = flags;
        SendInput(1, i, Marshal.SizeOf(typeof(INPUT)));
    }
    public static bool Force(IntPtr h) {
        IntPtr fg = GetForegroundWindow(); uint pid;
        uint t1 = GetWindowThreadProcessId(fg, out pid), t2 = GetCurrentThreadId();
        bool att = t1 != 0 && t1 != t2 && AttachThreadInput(t2, t1, true);
        ShowWindow(h, 9); bool ok = SetForegroundWindow(h);
        if (att) AttachThreadInput(t2, t1, false);
        return ok;
    }
}
'@
[void][W32]::SetProcessDpiAwarenessContext([IntPtr](-4))   # PerMonitorV2, so coordinates are real pixels

$tmp = Join-Path $env:TEMP ('ffe2e_' + [guid]::NewGuid().ToString('N').Substring(0, 8))
New-Item -ItemType Directory $tmp | Out-Null
$gameLog = Join-Path $tmp 'testgame.log'
$ffLog   = Join-Path $tmp 'freeframe.log'
$results = New-Object System.Collections.ArrayList
function Check($name, [bool]$ok, $detail = '') {
    [void]$results.Add([pscustomobject]@{ Name = $name; Ok = $ok; Detail = $detail })
    Write-Host ('[{0}] {1} {2}' -f ($(if ($ok) { 'PASS' } else { 'FAIL' })), $name, $detail)
}
function Read-LogShared($path) {
    if (-not (Test-Path $path)) { return @() }
    $fs = [IO.File]::Open($path, 'Open', 'Read', 'ReadWrite')
    try { $sr = New-Object IO.StreamReader($fs); $t = $sr.ReadToEnd() } finally { $fs.Dispose() }
    return ,($t -split "`r?`n")
}
function Sleep-Ms($ms) { Start-Sleep -Milliseconds $ms }
function Fg-Is($h) { [W32]::GetForegroundWindow() -eq $h }
function Click($x, $y, [switch]$Right) {
    [void][W32]::SetCursorPos($x, $y); Sleep-Ms 150
    if ($Right) { [W32]::Mouse(0x8); Sleep-Ms 60; [W32]::Mouse(0x10) } else { [W32]::Mouse(0x2); Sleep-Ms 60; [W32]::Mouse(0x4) }
    Sleep-Ms 200
}
function Press($vk) { [W32]::Key($vk, $false); Sleep-Ms 60; [W32]::Key($vk, $true); Sleep-Ms 150 }

$game = $null; $ff = $null; $warn = @()
try {
    foreach ($p in @($TestGame, $FreeFrame)) { if (-not (Test-Path $p)) { throw "Missing executable: $p" } }

    # 1-2. test game
    $game = Start-Process $TestGame -ArgumentList @('--size', '1280x720', '--pos', '40,40', '--fps', '60', '--log', "`"$gameLog`"", '--exit-after', '60') -PassThru
    Sleep-Ms 2000
    $game.Refresh(); $hg = $game.MainWindowHandle
    if ($hg -eq [IntPtr]::Zero) { throw 'testgame window not found' }
    [void][W32]::Force($hg); Sleep-Ms 400

    # 3. FreeFrame (retry: the exe may be getting rebuilt)
    for ($try = 1; $try -le 3 -and -not $ff; $try++) {
        try { $ff = Start-Process $FreeFrame -ArgumentList @('--target-title', '"FreeFrame Test Game"', '--exit-after', "$ExitAfter", '--log', "`"$ffLog`"") -PassThru }
        catch { Write-Host "FreeFrame start failed ($try/3): $($_.Exception.Message)"; Start-Sleep 15 }
    }
    if (-not $ff) { throw 'could not start FreeFrame' }
    Sleep-Ms 3000

    $pt = New-Object W32+POINT
    $rc = New-Object W32+RECT
    [void][W32]::GetClientRect($hg, [ref]$rc)
    [void][W32]::ClientToScreen($hg, [ref]$pt)
    $ox = $pt.X; $oy = $pt.Y; $cw = $rc.Right; $ch = $rc.Bottom
    Write-Host "game client: origin=($ox,$oy) size=${cw}x${ch}"
    Check 'testgame client area is 1280x720' ($cw -eq 1280 -and $ch -eq 720) "${cw}x${ch}"

    # 4a. foreground
    $fgSamples = 0; $fgOk = 0
    $sample = { $script:fgSamples++; if (Fg-Is $hg) { $script:fgOk++ } }
    & $sample
    Check 'game is foreground after FreeFrame starts' (Fg-Is $hg)
    $rcClip = New-Object W32+RECT
    [void][W32]::GetClipCursor([ref]$rcClip)
    Write-Host ("info: ClipCursor while scaling = ({0},{1},{2},{3})" -f $rcClip.Left, $rcClip.Top, $rcClip.Right, $rcClip.Bottom)

    # 4b. keys (A, Space, Enter)
    [void][W32]::SetCursorPos($ox + 640, $oy + 360); Sleep-Ms 200
    foreach ($vk in 0x41, 0x20, 0x0D) { Press $vk; & $sample }
    Check 'game still foreground after keys' (Fg-Is $hg)

    # 4c. clicks at 3 points
    $pts = @(@(100, 100, 'L'), @(900, 300, 'L'), @(640, 600, 'R'))
    foreach ($q in $pts) {
        if ($q[2] -eq 'R') { Click ($ox + $q[0]) ($oy + $q[1]) -Right } else { Click ($ox + $q[0]) ($oy + $q[1]) }
        & $sample
    }
    Check 'game still foreground after clicks' (Fg-Is $hg)

    # 4d. lose focus, return, click again
    $other = [W32]::GetShellWindow()
    [void][W32]::Force($other); Sleep-Ms 1200
    $lost = -not (Fg-Is $hg)
    Write-Host "info: foreground moved away from game: $lost"
    [void][W32]::Force($hg); Sleep-Ms 1200
    $back = Fg-Is $hg
    Check 'game regains foreground after focus loss' $back
    Press 0x42; Click ($ox + 300) ($oy + 500)   # 'B' + click after returning
    & $sample
    Sleep-Ms 500

    # wait for FreeFrame to exit
    if (-not $ff.WaitForExit(($ExitAfter + 10) * 1000)) { $warn += 'FreeFrame did not exit by itself; killed'; $ff.Kill(); $ff.WaitForExit() }
    Sleep-Ms 500

    # 5. report
    Check "game stayed foreground while active ($fgOk/$fgSamples samples)" ($fgOk -eq $fgSamples)

    $gl = Read-LogShared $gameLog
    $fl = Read-LogShared $ffLog
    foreach ($vk in 'A:0x41', 'Space:0x20', 'Enter:0x0D', 'B:0x42') {
        $n, $c = $vk.Split(':')
        $dn = @($gl | Where-Object { $_ -match "KEY down vk=$c\b" }).Count -ge 1
        $up = @($gl | Where-Object { $_ -match "KEY up vk=$c\b" }).Count -ge 1
        Check "key $n down+up received by game" ($dn -and $up)
    }
    $expect = @(@(100, 100, 'ldown'), @(900, 300, 'ldown'), @(640, 600, 'rdown'), @(300, 500, 'ldown'))
    foreach ($e in $expect) {
        $hit = $null
        foreach ($line in $gl) {
            if ($line -match "MOUSE $($e[2]) (-?\d+),(-?\d+)") {
                $mx = [int]$Matches[1]; $my = [int]$Matches[2]
                if ([math]::Abs($mx - $e[0]) -le 2 -and [math]::Abs($my - $e[1]) -le 2) { $hit = $line; break }
            }
        }
        Check ("mouse $($e[2]) at client ($($e[0]),$($e[1])) received (inside client)") ([bool]$hit) $(if ($hit) { $hit.Trim() } else { 'not found' })
    }
    $bad = @($gl | Where-Object { $_ -match 'MOUSE \w+ (-?\d+),(-?\d+)' -and ([int]$Matches[1] -lt 0 -or [int]$Matches[1] -ge 1280 -or [int]$Matches[2] -lt 0 -or [int]$Matches[2] -ge 720) })
    Check 'all mouse coordinates inside client area' ($bad.Count -eq 0) ($bad -join ' | ')

    $scal = $fl | Where-Object { $_ -match 'Scaling 1280x720 ->' } | Select-Object -First 1
    Check 'FreeFrame logged "Scaling 1280x720 -> ..."' ([bool]$scal) $scal

    $act0 = @($gl | Where-Object { $_ -match 'ACTIVATE 0 \(app\)' }).Count
    Write-Host "info: game saw ACTIVATE 0 (app) x$act0, FOCUS lost x$(@($gl | Where-Object { $_ -match 'FOCUS lost' }).Count), FOCUS gained x$(@($gl | Where-Object { $_ -match 'FOCUS gained' }).Count)"
    $stat = $gl | Where-Object { $_ -match 'STAT' } | Select-Object -Skip 4 -First 1
    Write-Host "info: $stat"

    # post-exit cursor state
    $cr = New-Object W32+RECT
    [void][W32]::GetClipCursor([ref]$cr)
    $vx = [W32]::GetSystemMetrics(76); $vy = [W32]::GetSystemMetrics(77)
    $vw = [W32]::GetSystemMetrics(78); $vh = [W32]::GetSystemMetrics(79)
    $full = $cr.Left -le $vx -and $cr.Top -le $vy -and $cr.Right -ge ($vx + $vw) -and $cr.Bottom -ge ($vy + $vh)
    Check 'after exit: ClipCursor = whole desktop' $full ("clip=({0},{1},{2},{3}) desktop=({4},{5},{6},{7})" -f $cr.Left, $cr.Top, $cr.Right, $cr.Bottom, $vx, $vy, ($vx + $vw), ($vy + $vh))
    $ci = New-Object W32+CURSORINFO; $ci.cbSize = [Runtime.InteropServices.Marshal]::SizeOf($ci)
    [void][W32]::GetCursorInfo([ref]$ci)
    Check 'after exit: cursor visible (CURSOR_SHOWING)' (($ci.flags -band 1) -ne 0) ("flags=0x{0:X}" -f $ci.flags)
}
catch {
    Check 'test harness' $false $_.Exception.Message
}
finally {
    # 6. cleanup
    foreach ($p in @($ff, $game)) { if ($p -and -not $p.HasExited) { try { $p.Kill() } catch {} } }
    Sleep-Ms 300
    $cr = New-Object W32+RECT; [void][W32]::GetClipCursor([ref]$cr)
    $ci = New-Object W32+CURSORINFO; $ci.cbSize = [Runtime.InteropServices.Marshal]::SizeOf($ci); [void][W32]::GetCursorInfo([ref]$ci)
    $vw = [W32]::GetSystemMetrics(78); $vh = [W32]::GetSystemMetrics(79)
    if (($cr.Right - $cr.Left) -lt $vw -or ($cr.Bottom - $cr.Top) -lt $vh) { Write-Host 'WARNING: cursor was still clipped; calling ClipCursor(NULL)'; [void][W32]::ClipCursor([IntPtr]::Zero) }
    if (($ci.flags -band 1) -eq 0) { Write-Host 'WARNING: cursor was hidden after cleanup' }
    foreach ($w in $warn) { Write-Host "WARNING: $w" }
    Write-Host "logs kept in $tmp"
}

$failed = @($results | Where-Object { -not $_.Ok }).Count
Write-Host ("`n== {0} checks, {1} failed ==" -f $results.Count, $failed)
exit ([int]($failed -gt 0))
