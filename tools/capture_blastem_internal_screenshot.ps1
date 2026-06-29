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
  [ValidateSet("P", "Enter", "Space", "Both")]
  [string]$StartKey = "P"
)

$ErrorActionPreference = "Stop"

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

function Send-Key([System.Diagnostics.Process]$Process, [byte]$VirtualKey, [byte]$ScanCode) {
  $handle = Get-BlastEmWindowHandle $Process
  [NativeInput]::ShowWindow($handle, 9) | Out-Null
  [NativeInput]::SetForegroundWindow($handle) | Out-Null

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
  if ($StartKey -eq "P") {
    Send-Key $Process 0x50 0x19
  }
  if ($StartKey -eq "Enter" -or $StartKey -eq "Both") {
    Send-EnterKey $Process
  }
  if ($StartKey -eq "Space" -or $StartKey -eq "Both") {
    Send-SpaceKey $Process
  }
}

function Send-ScreenshotKey([System.Diagnostics.Process]$Process) {
  Send-Key $Process 0x7B 0x58
}

$blastemExe = Resolve-ExistingPath (Join-Path $BlastEmDir "blastem.exe") "BlastEm executable"
$cuePath = Resolve-ExistingPath $Cue "CUE"
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
  $config = $config -replace "(?m)^(\s*)p\s+ui\.screenshot\s*$", "`$1p gamepads.1.start`r`n`$1f12 ui.screenshot"
  $config = $config -replace "(?m)^(\s*)enter\s+gamepads\.1\.start\s*$", "`$1enter gamepads.1.start`r`n`$1space gamepads.1.start"
  Set-Content -LiteralPath $configPath -Value $config -Encoding ASCII

  Add-Type @"
using System;
using System.Runtime.InteropServices;

public static class NativeInput {
  [DllImport("user32.dll")]
  public static extern bool PostMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

  [DllImport("user32.dll")]
  public static extern bool SetForegroundWindow(IntPtr hWnd);

  [DllImport("user32.dll")]
  public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
}
"@

  $before = @(Get-ChildItem -LiteralPath $OutputDir -Filter "*.png" -ErrorAction SilentlyContinue)
  $proc = Start-Process -FilePath $blastemExe `
    -ArgumentList @($cuePath) `
    -WorkingDirectory $BlastEmDir `
    -PassThru

  Start-Sleep -Seconds $SecondsBeforeStart
  Get-BlastEmWindowHandle $proc | Out-Null

  for ($i = 0; $i -lt $StartPresses; $i++) {
    Send-StartKey $proc
    if ($i -lt ($StartPresses - 1)) {
      Start-Sleep -Milliseconds $MillisecondsBetweenStartPresses
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
