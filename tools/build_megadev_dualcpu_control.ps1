param(
  [string]$SgdkBin = "C:\SDKS\SGDK\bin",
  [string]$Python = "",
  [string]$OutDir = "build\megadev_dualcpu_control",
  [ValidateSet("JP", "US", "EU")]
  [string]$Region = "US"
)

$ErrorActionPreference = "Stop"

function Resolve-Tool([string]$Path, [string]$Name) {
  $resolved = Resolve-Path -LiteralPath $Path -ErrorAction SilentlyContinue
  if (-not $resolved) {
    throw "$Name not found: $Path"
  }
  return $resolved.Path
}

function Invoke-Native {
  param(
    [string]$Exe,
    [string[]]$CommandArgs
  )

  Write-Output ("+ {0} {1}" -f $Exe, ($CommandArgs -join " "))
  & $Exe @CommandArgs
  if ($LASTEXITCODE -ne 0) {
    throw "Command failed with exit code ${LASTEXITCODE}: $Exe"
  }
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$controlDir = Join-Path $PSScriptRoot "controls\megadev_dualcpu"
$outPath = Join-Path $repoRoot $OutDir
New-Item -ItemType Directory -Force -Path $outPath | Out-Null

$as = Resolve-Tool (Join-Path $SgdkBin "as.exe") "m68k assembler"
$gcc = Resolve-Tool (Join-Path $SgdkBin "gcc.exe") "m68k gcc"
$ld = Resolve-Tool (Join-Path $SgdkBin "ld.exe") "m68k linker"
$objcopy = Resolve-Tool (Join-Path $SgdkBin "objcopy.exe") "m68k objcopy"
$size = Resolve-Tool (Join-Path $SgdkBin "size.exe") "m68k size"

if (-not $Python) {
  $pythonCandidates = @(
    "C:\Users\estee\AppData\Local\Programs\Python\Python312\python.exe",
    "C:\Users\estee\AppData\Local\Programs\Python\Python314\python.exe",
    "python"
  )
  foreach ($candidate in $pythonCandidates) {
    $cmd = Get-Command $candidate -ErrorAction SilentlyContinue
    if ($cmd) {
      $Python = $cmd.Source
      break
    }
  }
}
if (-not $Python) {
  throw "Python not found"
}

$ipObj = Join-Path $outPath "ip.o"
$ipSecurityObj = Join-Path $outPath "security.o"
$ipElf = Join-Path $outPath "ip.elf"
$ipBin = Join-Path $outPath "ip.bin"
$ipMap = Join-Path $outPath "ip.map"
$spObj = Join-Path $outPath "sp.o"
$spElf = Join-Path $outPath "sp.elf"
$spBin = Join-Path $outPath "sp.bin"
$spMap = Join-Path $outPath "sp.map"
$outBase = Join-Path $outPath "megadev_dualcpu_control"

Invoke-Native -Exe $as -CommandArgs @(
  "-m68000",
  "--register-prefix-optional",
  "-o", $ipObj,
  (Join-Path $controlDir "ip.s")
)
Invoke-Native -Exe $gcc -CommandArgs @(
  "-m68000",
  "-Wall",
  "-Wextra",
  "-O2",
  "-ffunction-sections",
  "-fdata-sections",
  "-B", "$SgdkBin/",
  "-DJP=1",
  "-DUS=2",
  "-DEU=3",
  "-DREGION=$Region",
  "-c", (Join-Path $repoRoot "src\main\security.c"),
  "-o", $ipSecurityObj
)
Invoke-Native -Exe $ld -CommandArgs @(
  "-T", (Join-Path $controlDir "ip.ld"),
  "-Map=$ipMap",
  "--gc-sections",
  "-o", $ipElf,
  $ipSecurityObj,
  $ipObj
)
Invoke-Native -Exe $objcopy -CommandArgs @("-O", "binary", $ipElf, $ipBin)
Invoke-Native -Exe $size -CommandArgs @($ipElf)

Invoke-Native -Exe $as -CommandArgs @(
  "-m68000",
  "--register-prefix-optional",
  "-o", $spObj,
  (Join-Path $controlDir "sp.s")
)
Invoke-Native -Exe $ld -CommandArgs @(
  "-T", (Join-Path $controlDir "sp.ld"),
  "-Map=$spMap",
  "--gc-sections",
  "-o", $spElf,
  $spObj
)
Invoke-Native -Exe $objcopy -CommandArgs @("-O", "binary", $spElf, $spBin)
Invoke-Native -Exe $size -CommandArgs @($spElf)

Invoke-Native -Exe $Python -CommandArgs @(
  (Join-Path $repoRoot "tools\build_iso.py"),
  $ipBin,
  $spBin,
  $outBase,
  $Region
)
Invoke-Native -Exe $Python -CommandArgs @(
  (Join-Path $repoRoot "tools\verify_disc.py"),
  "$outBase.iso",
  "--cue", "$outBase.cue",
  "--ip", $ipBin,
  "--sp", $spBin,
  "--region", $Region
)

Write-Output "control_ip=$ipBin"
Write-Output "control_sp=$spBin"
Write-Output "control_elf=$ipElf"
Write-Output "control_cue=$outBase.cue"
