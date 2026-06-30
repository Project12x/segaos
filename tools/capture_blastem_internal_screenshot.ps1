param(
  [string]$BlastEmDir = "C:\tmp\blastem\blastem-win64-0.6.3-pre-ec47c727cd65",
  [string]$Cue = "build\segaos.cue",
  [string]$OutputDir = "C:\tmp\segaos_screens_internal",
  [string]$Template = "segaos_internal_%Y%m%d_%H%M%S.png",
  [int]$SecondsBeforeStart = 4,
  [int]$SecondsAfterStart = 20,
  [int]$StartPresses = 2,
  [int]$MillisecondsBetweenStartPresses = 1200,
  [int]$ScreenshotPresses = 3,
  [int]$MillisecondsBetweenScreenshotPresses = 500,
  [int]$FocusTimeoutSeconds = 8,
  [ValidateSet("PostMessage", "SendInputGuarded")]
  [string]$InputMode = "PostMessage",
  [switch]$ClickToFocus,
  [ValidateSet("P", "Enter", "Space", "All")]
  [string]$StartKey = "All",
  [ValidateSet("P", "F12")]
  [string]$ScreenshotKey = "P",
  [switch]$DebugAutoBoot,
  [string]$Elf = "build\main_cpu.elf",
  [string]$Gdb = "C:\SDKS\SGDK\bin\gdb.exe",
  [string]$DebugBreakSymbol = "segaos_visual_probe_halt"
)

$ErrorActionPreference = "Stop"

if ($ScreenshotKey -eq "P" -and $StartKey -eq "P") {
  throw "ScreenshotKey P conflicts with StartKey P; use StartKey Enter, Space, or All"
}

function Resolve-ExistingPath([string]$Path, [string]$Label) {
  $resolved = Resolve-Path -LiteralPath $Path -ErrorAction SilentlyContinue
  if (-not $resolved) {
    throw "$Label not found: $Path"
  }
  return $resolved.Path
}

function Get-BlastEmWindowHandle([System.Diagnostics.Process]$Process) {
  if (-not $Process -or $Process.HasExited) {
    throw "BlastEm process is not running"
  }

  $Process.Refresh()
  $deadline = (Get-Date).AddSeconds($FocusTimeoutSeconds)
  while ($Process.MainWindowHandle -eq 0 -and (Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 100
    $Process.Refresh()
  }

  if ($Process.MainWindowHandle -eq 0) {
    throw "BlastEm did not create a window handle before the timeout"
  }

  return $Process.MainWindowHandle
}

function Set-BlastEmForeground([System.Diagnostics.Process]$Process, [IntPtr]$Handle) {
  [NativeInput]::ShowWindow($Handle, 9) | Out-Null
  [NativeInput]::SetForegroundWindow($Handle) | Out-Null

  $shell = New-Object -ComObject WScript.Shell
  $shell.AppActivate($Process.Id) | Out-Null

  if ($ClickToFocus) {
    $hwndTopMost = [IntPtr](-1)
    $hwndNoTopMost = [IntPtr](-2)
    [NativeInput]::SetWindowPos($Handle, $hwndTopMost, 0, 0, 0, 0, 0x0001 -bor 0x0002) | Out-Null
    $rect = New-Object NativeInput+RECT
    if (-not [NativeInput]::GetWindowRect($Handle, [ref]$rect)) {
      throw "Failed to query BlastEm window rectangle"
    }
    $x = [int](($rect.Left + $rect.Right) / 2)
    $y = [int](($rect.Top + $rect.Bottom) / 2)
    [NativeInput]::SetCursorPos($x, $y) | Out-Null
    [NativeInput]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 60
    [NativeInput]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
    [NativeInput]::SetWindowPos($Handle, $hwndNoTopMost, 0, 0, 0, 0, 0x0001 -bor 0x0002) | Out-Null
  }

  $deadline = (Get-Date).AddSeconds($FocusTimeoutSeconds)
  while ([NativeInput]::GetForegroundWindow() -ne $Handle -and (Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 100
    [NativeInput]::ShowWindow($Handle, 9) | Out-Null
    [NativeInput]::SetForegroundWindow($Handle) | Out-Null
    $shell.AppActivate($Process.Id) | Out-Null
  }
}

function Send-Key([System.Diagnostics.Process]$Process, [byte]$VirtualKey, [byte]$ScanCode) {
  $handle = Get-BlastEmWindowHandle $Process
  Set-BlastEmForeground $Process $handle

  if ($InputMode -eq "SendInputGuarded") {
    Start-Sleep -Milliseconds 120
    if ([NativeInput]::GetForegroundWindow() -ne $handle) {
      throw "Refusing to send input because BlastEm is not the foreground window"
    }
    [NativeInput]::SendKeyboardInput($VirtualKey, $ScanCode, $false)
    Start-Sleep -Milliseconds 80
    if ([NativeInput]::GetForegroundWindow() -ne $handle) {
      throw "Refusing to release input because BlastEm lost foreground focus"
    }
    [NativeInput]::SendKeyboardInput($VirtualKey, $ScanCode, $true)
    return
  }

  $keyDownLParam = [IntPtr](1 -bor ([int]$ScanCode -shl 16))
  $keyUpLParam = [IntPtr](1 -bor ([int]$ScanCode -shl 16) -bor 0xC0000000)

  if (-not [NativeInput]::PostMessage($handle, 0x0100, [IntPtr]$VirtualKey, $keyDownLParam)) {
    throw "Failed to post key-down message to BlastEm window"
  }
  Start-Sleep -Milliseconds 80
  if (-not [NativeInput]::PostMessage($handle, 0x0101, [IntPtr]$VirtualKey, $keyUpLParam)) {
    throw "Failed to post key-up message to BlastEm window"
  }
}

function Send-EnterKey([System.Diagnostics.Process]$Process) {
  Send-Key $Process 0x0D 0x1C
}

function Send-SpaceKey([System.Diagnostics.Process]$Process) {
  Send-Key $Process 0x20 0x39
}

function Send-StartKey([System.Diagnostics.Process]$Process) {
  if (($StartKey -eq "P" -or $StartKey -eq "All") -and $ScreenshotKey -ne "P") {
    Send-Key $Process 0x50 0x19
  }
  if ($StartKey -eq "Enter" -or $StartKey -eq "All") {
    Send-EnterKey $Process
  }
  if ($StartKey -eq "Space" -or $StartKey -eq "All") {
    Send-SpaceKey $Process
  }
}

function Send-ScreenshotKey([System.Diagnostics.Process]$Process) {
  if ($ScreenshotKey -eq "P") {
    Send-Key $Process 0x50 0x19
  } else {
    Send-Key $Process 0x7B 0x58
  }
}

$blastemExe = Resolve-ExistingPath (Join-Path $BlastEmDir "blastem.exe") "BlastEm executable"
$cuePath = Resolve-ExistingPath $Cue "CUE"
$elfPath = $null
$gdbPath = $null
if ($DebugAutoBoot) {
  $elfPath = Resolve-ExistingPath $Elf "Main CPU ELF"
  $gdbPath = Resolve-ExistingPath $Gdb "m68k GDB"
}
$defaultConfigPath = Resolve-ExistingPath (Join-Path $BlastEmDir "default.cfg") "BlastEm default config"
$configDir = Join-Path $env:LOCALAPPDATA "blastem"
$configPath = Join-Path $configDir "blastem.cfg"
$backupPath = Join-Path $configDir "blastem.cfg.segaos-capture.bak"
$hadConfig = Test-Path -LiteralPath $configPath

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
New-Item -ItemType Directory -Force -Path $configDir | Out-Null

if ($hadConfig) {
  Copy-Item -LiteralPath $configPath -Destination $backupPath -Force
}

$outputDirForConfig = ($OutputDir -replace "\\", "/")

$proc = $null
try {
  $config = Get-Content -LiteralPath $defaultConfigPath -Raw
  $config = $config -replace "(?m)^(\s*)screenshot_path\s+.*$", "`$1screenshot_path $outputDirForConfig"
  $config = $config -replace "(?m)^(\s*)screenshot_template\s+.*$", "`$1screenshot_template $Template"
  if ($ScreenshotKey -eq "F12") {
    $config = $config -replace "(?m)^(\s*)p\s+ui\.screenshot\s*$", "`$1p gamepads.1.start`r`n`$1f12 ui.screenshot"
  }
  $config = $config -replace "(?m)^(\s*)enter\s+gamepads\.1\.start\s*$", "`$1enter gamepads.1.start`r`n`$1space gamepads.1.start"
  Set-Content -LiteralPath $configPath -Value $config -Encoding ASCII

  Add-Type @"
using System;
using System.Runtime.InteropServices;

public static class NativeInput {
  [StructLayout(LayoutKind.Sequential)]
  public struct RECT {
    public int Left;
    public int Top;
    public int Right;
    public int Bottom;
  }

  [DllImport("user32.dll")]
  public static extern bool PostMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

  [DllImport("user32.dll")]
  public static extern bool SetForegroundWindow(IntPtr hWnd);

  [DllImport("user32.dll")]
  public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

  [DllImport("user32.dll")]
  public static extern IntPtr GetForegroundWindow();

  [DllImport("user32.dll")]
  public static extern bool GetWindowRect(IntPtr hWnd, ref RECT lpRect);

  [DllImport("user32.dll")]
  public static extern bool SetCursorPos(int X, int Y);

  [DllImport("user32.dll")]
  public static extern void mouse_event(uint dwFlags, uint dx, uint dy, uint dwData, UIntPtr dwExtraInfo);

  [DllImport("user32.dll")]
  public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);

  [DllImport("user32.dll")]
  public static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, UIntPtr dwExtraInfo);

  public static void SendKeyboardInput(byte virtualKey, byte scanCode, bool keyUp) {
    keybd_event(virtualKey, scanCode, keyUp ? 0x0002u : 0u, UIntPtr.Zero);
  }
}
"@

  $before = @(Get-ChildItem -LiteralPath $OutputDir -Filter "*.png" -ErrorAction SilentlyContinue)
  $blastemArgs = @($cuePath)
  if ($DebugAutoBoot) {
    $blastemArgs += "-D"
  }

  $proc = Start-Process -FilePath $blastemExe `
    -ArgumentList $blastemArgs `
    -WorkingDirectory $BlastEmDir `
    -PassThru

  Start-Sleep -Seconds $SecondsBeforeStart
  if (-not $DebugAutoBoot) {
    Get-BlastEmWindowHandle $proc | Out-Null
  }

  if ($DebugAutoBoot) {
    $gdbArgs = @(
      "-q",
      "-batch",
      "-ex",
      "set mi-async on",
      "-ex",
      "set tcp connect-timeout 5",
      "-ex",
      "set remotetimeout 10",
      "-ex",
      "target remote 127.0.0.1:1234",
      "-ex",
      "break $DebugBreakSymbol",
      "-ex",
      "continue",
      "-ex",
      "p/x (unsigned short)segaos_visual_probe_phase",
      # Keep BlastEm running after the proof point; a stopped GDB target does
      # not reliably process the UI screenshot binding.
      "-ex",
      "delete breakpoints",
      "-ex",
      "continue&",
      "-ex",
      "disconnect",
      $elfPath
    )
    $oldErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
      $gdbOutput = & $gdbPath @gdbArgs 2>&1 | ForEach-Object { $_.ToString() }
      $gdbExit = $LASTEXITCODE
    } finally {
      $ErrorActionPreference = $oldErrorActionPreference
    }
    $hitBreakpoint = (($gdbOutput -join "`n") -match "Breakpoint \d+, .*${DebugBreakSymbol}")
    $phaseOk = (($gdbOutput -join "`n") -match "0x76ff")
    Write-Output "debug_gdb_exit=$gdbExit"
    Write-Output "debug_breakpoint_hit=$hitBreakpoint"
    Write-Output "debug_phase_ok=$phaseOk"
    if ($gdbExit -ne 0 -or -not $hitBreakpoint -or -not $phaseOk) {
      $gdbOutput | ForEach-Object { Write-Output $_ }
      throw "Debug auto-boot did not reach $DebugBreakSymbol with phase 0x76ff"
    }
    Get-BlastEmWindowHandle $proc | Out-Null
  } else {
    for ($i = 0; $i -lt $StartPresses; $i++) {
      Send-StartKey $proc
      if ($i -lt ($StartPresses - 1)) {
        Start-Sleep -Milliseconds $MillisecondsBetweenStartPresses
      }
    }
  }
  Start-Sleep -Seconds $SecondsAfterStart
  Get-BlastEmWindowHandle $proc | Out-Null
  for ($i = 0; $i -lt $ScreenshotPresses; $i++) {
    Send-ScreenshotKey $proc
    if ($i -lt ($ScreenshotPresses - 1)) {
      Start-Sleep -Milliseconds $MillisecondsBetweenScreenshotPresses
    }
  }
  Start-Sleep -Seconds 1

  $beforeNames = @{}
  foreach ($file in $before) {
    $beforeNames[$file.FullName] = $true
  }

  $after = Get-ChildItem -LiteralPath $OutputDir -Filter "*.png" |
    Where-Object { -not $beforeNames.ContainsKey($_.FullName) } |
    Sort-Object LastWriteTime -Descending

  if (-not $after) {
    throw "BlastEm did not create a new internal screenshot in $OutputDir"
  }

  $shot = $after | Select-Object -First 1
  Write-Output "screenshot=$($shot.FullName)"
  Write-Output "bytes=$($shot.Length)"
}
finally {
  if ($proc -and -not $proc.HasExited) {
    Stop-Process -Id $proc.Id -Force
  }

  if ($hadConfig) {
    Move-Item -LiteralPath $backupPath -Destination $configPath -Force
  } elseif (Test-Path -LiteralPath $configPath) {
    Remove-Item -LiteralPath $configPath -Force
  }
}
