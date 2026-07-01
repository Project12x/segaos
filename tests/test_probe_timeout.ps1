param(
  [string]$HostCc = "C:/Qt/Tools/mingw1310_64/bin/gcc.exe"
)

$ErrorActionPreference = "Stop"

function Fail([string]$Message) {
  Write-Output "FAIL: $Message"
  exit 1
}

$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$buildDir = Join-Path $root "build\probe_timeout_test"
$probeScript = Join-Path $root "tools\probe_blastem_boot.ps1"
$fakeBlastemSrc = Join-Path $buildDir "fake_blastem.c"
$fakeGdbSrc = Join-Path $buildDir "fake_gdb.c"
$fakeBlastemExe = Join-Path $buildDir "blastem.exe"
$fakeGdbExe = Join-Path $buildDir "fake_gdb.exe"
$cuePath = Join-Path $buildDir "dummy.cue"
$elfPath = Join-Path $buildDir "dummy.elf"
$biosPath = Join-Path $buildDir "cdbios.bin"

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

$fakeProcessSource = @'
#include <windows.h>

int main(void) {
  Sleep(30000);
  return 0;
}
'@

Set-Content -LiteralPath $fakeBlastemSrc -Value $fakeProcessSource -Encoding ASCII
Set-Content -LiteralPath $fakeGdbSrc -Value $fakeProcessSource -Encoding ASCII
Set-Content -LiteralPath $cuePath -Value "FILE ""dummy.iso"" BINARY`r`n  TRACK 01 MODE1/2048`r`n" -Encoding ASCII
Set-Content -LiteralPath $elfPath -Value "dummy" -Encoding ASCII
Set-Content -LiteralPath $biosPath -Value "dummy" -Encoding ASCII

& $HostCc $fakeBlastemSrc -o $fakeBlastemExe
if ($LASTEXITCODE -ne 0) {
  Fail "could not compile fake BlastEm"
}

& $HostCc $fakeGdbSrc -o $fakeGdbExe
if ($LASTEXITCODE -ne 0) {
  Fail "could not compile fake GDB"
}

$powershellExe = (Get-Command powershell.exe).Source
$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
$output = & $powershellExe -NoProfile -ExecutionPolicy Bypass -File $probeScript `
  -BlastEmDir $buildDir `
  -Cue $cuePath `
  -Elf $elfPath `
  -Gdb $fakeGdbExe `
  -Probe Ip `
  -GdbTimeoutSeconds 2 2>&1 | ForEach-Object { $_.ToString() }
$exitCode = $LASTEXITCODE
$stopwatch.Stop()
$joined = $output -join "`n"

if ($exitCode -eq 0) {
  Fail "probe script unexpectedly succeeded with hung fake GDB"
}

if ($stopwatch.Elapsed.TotalSeconds -gt 12) {
  Fail ("probe timeout took too long: {0:n1}s" -f $stopwatch.Elapsed.TotalSeconds)
}

if ($joined -notmatch "probe_gdb_timeout=True") {
  $output | ForEach-Object { Write-Output $_ }
  Fail "probe script did not report its own GDB timeout"
}

Write-Output "probe timeout test passed"
