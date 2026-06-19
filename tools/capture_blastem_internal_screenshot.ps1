param(
  [string]$BlastEmDir = "C:\tmp\blastem\blastem-win64-0.6.3-pre-ec47c727cd65",
  [string]$Cue = "build\segaos.cue",
  [string]$OutputDir = "C:\tmp\segaos_screens_internal",
  [string]$Template = "segaos_internal_%Y%m%d_%H%M%S.png",
  [int]$SecondsBeforeStart = 4,
  [int]$SecondsAfterStart = 4,
  [int]$StartPresses = 2,
  [int]$MillisecondsBetweenStartPresses = 1200
)

$ErrorActionPreference = "Stop"

function Resolve-ExistingPath([string]$Path, [string]$Label) {
  $resolved = Resolve-Path -LiteralPath $Path -ErrorAction SilentlyContinue
  if (-not $resolved) {
    throw "$Label not found: $Path"
  }
  return $resolved.Path
}

function Send-Key([byte]$VirtualKey, [byte]$ScanCode) {
  [NativeInput]::keybd_event($VirtualKey, $ScanCode, 0, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 80
  [NativeInput]::keybd_event($VirtualKey, $ScanCode, 2, [UIntPtr]::Zero)
}

function Send-EnterKey() {
  Send-Key 0x0D 0x1C
  try {
    $shell = New-Object -ComObject WScript.Shell
    $shell.SendKeys("~")
  } catch {
    # keybd_event above is the primary path; SendKeys is a best-effort fallback.
  }
}

function Send-ScreenshotKey() {
  Send-Key 0x50 0x19
  try {
    $shell = New-Object -ComObject WScript.Shell
    $shell.SendKeys("p")
  } catch {
  }
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
  Set-Content -LiteralPath $configPath -Value $config -Encoding ASCII

  Add-Type @"
using System;
using System.Runtime.InteropServices;

public static class NativeInput {
  [DllImport("user32.dll")]
  public static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, UIntPtr dwExtraInfo);

  [DllImport("user32.dll")]
  public static extern bool SetForegroundWindow(IntPtr hWnd);
}
"@

  $before = @(Get-ChildItem -LiteralPath $OutputDir -Filter "*.png" -ErrorAction SilentlyContinue)
  $proc = Start-Process -FilePath $blastemExe `
    -ArgumentList @($cuePath) `
    -WorkingDirectory $BlastEmDir `
    -PassThru

  Start-Sleep -Seconds $SecondsBeforeStart
  if ($proc.MainWindowHandle -ne 0) {
    [NativeInput]::SetForegroundWindow($proc.MainWindowHandle) | Out-Null
    Start-Sleep -Milliseconds 250
  }

  for ($i = 0; $i -lt $StartPresses; $i++) {
    Send-EnterKey
    if ($i -lt ($StartPresses - 1)) {
      Start-Sleep -Milliseconds $MillisecondsBetweenStartPresses
    }
  }
  Start-Sleep -Seconds $SecondsAfterStart
  if ($proc.MainWindowHandle -ne 0) {
    [NativeInput]::SetForegroundWindow($proc.MainWindowHandle) | Out-Null
    Start-Sleep -Milliseconds 250
  }
  Send-ScreenshotKey
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
