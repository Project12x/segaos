param(
  [string]$BlastEmDir = "C:\tmp\blastem\blastem-win64-0.6.3-pre-ec47c727cd65",
  [string]$Bios = "",
  [string]$Cue = "build\segaos.cue",
  [string]$Elf = "build\main_cpu.elf",
  [string]$Gdb = "C:\SDKS\SGDK\bin\gdb.exe",
  [string]$IpAddress = "0xff0000",
  [string]$ExpectedPrefix = "43fa000a4eb80364",
  [ValidateSet("Ip", "DualCpu", "DualCpuStatus", "DualCpuWramSurvey", "DualCpuWramRetClear", "DualCpuWramSweep", "Framebuffer", "MegadevControl")]
  [string]$Probe = "Ip"
)

$ErrorActionPreference = "Stop"

function Resolve-ExistingPath([string]$Path, [string]$Label) {
  $resolved = Resolve-Path -LiteralPath $Path -ErrorAction SilentlyContinue
  if (-not $resolved) {
    throw "$Label not found: $Path"
  }
  return $resolved.Path
}

function Get-HexBytesFromGdbLine([string[]]$Output, [string]$Address) {
  $addr = ($Address -replace "^0x", "").TrimStart("0")
  if ($addr.Length -eq 0) {
    $addr = "0"
  }

  $line = $Output | Where-Object {
    $_.ToLowerInvariant() -match "^\s*0x0*$addr\b.*:"
  } | Select-Object -First 1

  if (-not $line) {
    return ""
  }

  $byteFields = ($line -split ":", 2)[1]
  $matches = [regex]::Matches($byteFields.ToLowerInvariant(), "0x([0-9a-f]{2})")
  return (($matches | ForEach-Object { $_.Groups[1].Value }) -join "")
}

function Get-ProbeValue([string[]]$Output, [string]$Name) {
  $line = $Output | Where-Object {
    $_.ToLowerInvariant().StartsWith("$($Name.ToLowerInvariant())=")
  } | Select-Object -Last 1

  if (-not $line) {
    return ""
  }

  return (($line -split "=", 2)[1]).Trim().ToLowerInvariant()
}

function Get-NamedProbeValues([string[]]$Output, [string[]]$Names) {
  $values = @{}
  foreach ($name in $Names) {
    $value = Get-ProbeValue $Output $name
    if ($value -and $value -match '^\$\d+\s+=\s+(0x[0-9a-fA-F]+|\d+)\s*$') {
      $value = $Matches[1].ToLowerInvariant()
    }
    if ($value -and $value -match '^(0x[0-9a-fA-F]+|\d+)$') {
      if ($value -notmatch '^0x') {
        $value = ('0x{0:x4}' -f [int]$value)
      }
      elseif ($value -match '^0x([0-9a-f]+)$') {
        $value = ('0x{0:x4}' -f [Convert]::ToInt32($Matches[1], 16))
      }
    } else {
      $value = ""
    }

    if (-not $value) {
      $prefix = "$($name.ToLowerInvariant())=$"
      $line = $Output | Where-Object {
        $_.ToLowerInvariant().StartsWith($prefix)
      } | Select-Object -Last 1

      if ($line -and $line -match '=\s+(0x[0-9a-fA-F]+|\d+)\s*$') {
        $value = $Matches[1].ToLowerInvariant()
        if ($value -notmatch '^0x') {
          $value = ('0x{0:x4}' -f [int]$value)
        }
        elseif ($value -match '^0x([0-9a-f]+)$') {
          $value = ('0x{0:x4}' -f [Convert]::ToInt32($Matches[1], 16))
        }
      }
    }
    $values[$name] = $value
  }
  return $values
}

function Invoke-Gdb([string[]]$Commands) {
  $oldErrorActionPreference = $ErrorActionPreference
  $ErrorActionPreference = "Continue"
  try {
    $args = @("-q", "-batch")
    foreach ($command in $Commands) {
      $args += @("-ex", $command)
    }
    $args += $elfPath
    $output = & $gdbPath @args 2>&1 | ForEach-Object { $_.ToString() }
    $exitCode = $LASTEXITCODE
    return @{ Output = $output; ExitCode = $exitCode }
  }
  finally {
    $ErrorActionPreference = $oldErrorActionPreference
  }
}

$blastemExe = Join-Path $BlastEmDir "blastem.exe"
$blastemExe = Resolve-ExistingPath $blastemExe "BlastEm executable"
$cuePath = Resolve-ExistingPath $Cue "CUE"
$elfPath = Resolve-ExistingPath $Elf "Main CPU ELF"
$gdbPath = Resolve-ExistingPath $Gdb "m68k GDB"

$cdbios = Join-Path $BlastEmDir "cdbios.bin"
if ($Bios) {
  $biosPath = Resolve-ExistingPath $Bios "Sega CD BIOS"
  Copy-Item -LiteralPath $biosPath -Destination $cdbios -Force
}
$cdbios = Resolve-ExistingPath $cdbios "BlastEm cdbios.bin"

$proc = $null
try {
  $proc = Start-Process -FilePath $blastemExe `
    -ArgumentList @($cuePath, "-D") `
    -WorkingDirectory $BlastEmDir `
    -WindowStyle Hidden `
    -PassThru

  Start-Sleep -Seconds 2

  $gdbCommands = @(
    "set tcp connect-timeout 5",
    "set remotetimeout 10",
    "target remote 127.0.0.1:1234"
  )

  if ($Probe -eq "DualCpu" -or $Probe -eq "DualCpuStatus" -or $Probe -eq "DualCpuWramSurvey" -or $Probe -eq "DualCpuWramRetClear" -or $Probe -eq "DualCpuWramSweep" -or $Probe -eq "Framebuffer") {
    $probeNames = @(
      "probe_main_magic",
      "probe_main_phase",
      "probe_sub_state",
      "probe_sub_ready_magic",
      "probe_comm_status",
      "probe_sub_result0",
      "probe_sub_result1",
      "probe_sub_result2",
      "probe_sub_result3",
      "probe_sub_result4",
      "probe_sub_result5",
      "probe_sub_result6",
      "probe_sub_trace",
      "probe_wram_word0",
      "probe_wram_word1",
      "probe_wram_bank1_word0",
      "probe_wram_bank1_word1",
      "probe_wram_mcd1_bank0_word0",
      "probe_wram_mcd1_bank0_word1",
      "probe_wram_mcd1_bank1_word0",
      "probe_wram_mcd1_bank1_word1",
      "probe_wram_survey",
      "probe_mem_mode_before",
      "probe_mem_mode_after",
      "probe_mem_mode_after_wram",
      "probe_fb_wram_row0_word0",
      "probe_fb_wram_row0_word1",
      "probe_fb_wram_row1_word0",
      "probe_fb_wram_row1_word1",
      "probe_fb_vram_word0",
      "probe_fb_vram_word1",
      "probe_fb_vram_word2",
      "probe_fb_vram_word3"
    )

    if ($Probe -eq "DualCpu" -or $Probe -eq "DualCpuWramSurvey" -or $Probe -eq "DualCpuWramRetClear" -or $Probe -eq "DualCpuWramSweep" -or $Probe -eq "Framebuffer") {
      $surveyMode = "1"
      if ($Probe -eq "DualCpu" -or $Probe -eq "DualCpuWramRetClear") {
        $surveyMode = "2"
      }
      if ($Probe -eq "Framebuffer") {
        $surveyMode = "3"
      }
      $gdbCommands += @(
        "break main",
        "continue",
        "set {unsigned short}&segaos_probe_wram_survey = $surveyMode"
      )
    }

    $gdbCommands += @(
      "break segaos_probe_halt",
      "continue",
      "info registers pc sp"
    )
    if ($Probe -eq "DualCpuWramSweep") {
      foreach ($memMode in @("0x04", "0x05", "0x06", "0x07")) {
        $gdbCommands += @(
          "set {unsigned char}0xa12003 = $memMode",
          "echo sweep_memmode_$memMode`n",
          "p/x *(unsigned short *)0xa12002",
          "echo sweep_200000_$memMode`n",
          "x/4hx 0x200000",
          "echo sweep_220000_$memMode`n",
          "x/4hx 0x220000",
          "echo sweep_600000_$memMode`n",
          "x/4hx 0x600000",
          "echo sweep_620000_$memMode`n",
          "x/4hx 0x620000"
        )
      }
    }
    foreach ($name in $probeNames) {
      $symbol = "segaos_$name"
      $gdbCommands += "echo $name="
      $gdbCommands += "p/x (unsigned short)$symbol"
    }
  } elseif ($Probe -eq "MegadevControl") {
    $controlNames = @(
      "megadev_control_main_magic",
      "megadev_control_phase",
      "megadev_control_sub_flag",
      "megadev_control_stat0",
      "megadev_control_stat1",
      "megadev_control_stat2",
      "megadev_control_stat3",
      "megadev_control_trace"
    )

    $gdbCommands += @(
      "break megadev_control_halt",
      "continue",
      "info registers pc sp"
    )
    foreach ($name in $controlNames) {
      $gdbCommands += "echo $name="
      $gdbCommands += "p/x (unsigned short)$name"
    }
  } else {
    $gdbCommands += @(
      "break *$IpAddress",
      "continue",
      "info registers pc sp",
      "x/16xb $IpAddress"
    )
  }

  $gdbResult = Invoke-Gdb $gdbCommands
  $gdbOutput = $gdbResult.Output
  $gdbExit = $gdbResult.ExitCode

  $gdbOutput | ForEach-Object { Write-Output $_ }

  if ($Probe -eq "DualCpu" -or $Probe -eq "DualCpuStatus" -or $Probe -eq "DualCpuWramSurvey" -or $Probe -eq "DualCpuWramRetClear" -or $Probe -eq "DualCpuWramSweep" -or $Probe -eq "Framebuffer") {
    $joined = $gdbOutput -join "`n"
    $hitBreakpoint = ($joined -match "Breakpoint \d+, .*segaos_probe_halt")
    $probeValues = Get-NamedProbeValues $gdbOutput $probeNames
    $expectedValues = [ordered]@{
      probe_main_magic = "0x4d50"
      probe_sub_state = "0x0002"
      probe_sub_ready_magic = "0x5244"
      probe_comm_status = "0x0003"
      probe_sub_result0 = "0x444e"
      probe_sub_result1 = "0xa55a"
      probe_sub_result2 = "0x5aa5"
      probe_sub_trace = "0x52fe"
    }
    if ($Probe -eq "DualCpuWramSurvey" -or $Probe -eq "DualCpuWramSweep") {
      $expectedValues["probe_sub_trace"] = "0x53fe"
    }
    if ($Probe -eq "DualCpu" -or $Probe -eq "DualCpuWramRetClear") {
      $expectedValues["probe_sub_trace"] = "0x54fe"
    }
    if ($Probe -eq "Framebuffer") {
      $expectedValues["probe_sub_trace"] = "0x55fe"
      $expectedValues["probe_main_phase"] = "0x00ff"
      $expectedValues["probe_wram_word0"] = "0x1234"
      $expectedValues["probe_wram_word1"] = "0x5678"
      $expectedValues["probe_fb_wram_row0_word0"] = "0x1234"
      $expectedValues["probe_fb_wram_row0_word1"] = "0x5678"
      $expectedValues["probe_fb_wram_row1_word0"] = "0x9abc"
      $expectedValues["probe_fb_wram_row1_word1"] = "0xdef0"
    }
    if ($Probe -eq "DualCpu") {
      $expectedValues["probe_main_phase"] = "0x00ff"
      $expectedValues["probe_wram_word0"] = "0xa55a"
      $expectedValues["probe_wram_word1"] = "0x5aa5"
    }

    $failed = @()
    foreach ($name in $expectedValues.Keys) {
      $actualValue = $probeValues[$name]
      $ok = $actualValue -eq $expectedValues[$name]
      Write-Output "probe_check_$name=$ok expected=$($expectedValues[$name]) actual=$actualValue"
      if (-not $ok) {
        $failed += $name
      }
    }

    if ($Probe -eq "Framebuffer") {
      $subBank0Ok = (
        $probeValues["probe_sub_result4"] -eq "0x1234" -and
        $probeValues["probe_sub_result5"] -eq "0x5678"
      )
      Write-Output "probe_sub_wram0_match=$subBank0Ok expected=0x1234,0x5678 actual=$($probeValues["probe_sub_result4"]),$($probeValues["probe_sub_result5"])"
    } elseif ($Probe -eq "DualCpu" -or $Probe -eq "DualCpuWramSurvey" -or $Probe -eq "DualCpuWramRetClear" -or $Probe -eq "DualCpuWramSweep") {
      $subBank0Ok = (
        $probeValues["probe_sub_result4"] -eq "0xa55a" -and
        $probeValues["probe_sub_result5"] -eq "0x5aa5"
      )
      Write-Output "probe_sub_wram0_match=$subBank0Ok expected=0xa55a,0x5aa5 actual=$($probeValues["probe_sub_result4"]),$($probeValues["probe_sub_result5"])"
    }

    if ($Probe -eq "DualCpu" -or $Probe -eq "DualCpuWramRetClear") {
      $subResult6IsRetClearOk = ($probeValues["probe_sub_result6"] -eq "0x2a04")
      $mainBank0Ok = (
        $probeValues["probe_wram_word0"] -eq "0xa55a" -and
        $probeValues["probe_wram_word1"] -eq "0x5aa5"
      )
      $mainBank1MirrorOk = (
        $probeValues["probe_wram_bank1_word0"] -eq "0xa55a" -and
        $probeValues["probe_wram_bank1_word1"] -eq "0x5aa5"
      )
      $mainMcd1Bank0IdleOk = (
        $probeValues["probe_wram_mcd1_bank0_word0"] -eq "0x3300" -and
        $probeValues["probe_wram_mcd1_bank0_word1"] -eq "0x3300"
      )
      $mainMcd1Bank1IdleOk = (
        $probeValues["probe_wram_mcd1_bank1_word0"] -eq "0x3300" -and
        $probeValues["probe_wram_mcd1_bank1_word1"] -eq "0x3300"
      )
      Write-Output "probe_sub_ret_clear_mem_mode=$subResult6IsRetClearOk expected=0x2a04 actual=$($probeValues["probe_sub_result6"])"
      Write-Output "probe_main_wram_bank0_match=$mainBank0Ok expected=0xa55a,0x5aa5 actual=$($probeValues["probe_wram_word0"]),$($probeValues["probe_wram_word1"])"
      Write-Output "probe_main_wram_bank1_mirror=$mainBank1MirrorOk expected=0xa55a,0x5aa5 actual=$($probeValues["probe_wram_bank1_word0"]),$($probeValues["probe_wram_bank1_word1"])"
      Write-Output "probe_main_wram_mcd1_bank0_idle=$mainMcd1Bank0IdleOk expected=0x3300,0x3300 actual=$($probeValues["probe_wram_mcd1_bank0_word0"]),$($probeValues["probe_wram_mcd1_bank0_word1"])"
      Write-Output "probe_main_wram_mcd1_bank1_idle=$mainMcd1Bank1IdleOk expected=0x3300,0x3300 actual=$($probeValues["probe_wram_mcd1_bank1_word0"]),$($probeValues["probe_wram_mcd1_bank1_word1"])"
    } elseif ($Probe -eq "Framebuffer") {
      $fbTileRow0Ok = (
        $probeValues["probe_fb_vram_word0"] -eq "0x1234" -and
        $probeValues["probe_fb_vram_word1"] -eq "0x5678"
      )
      $fbTileRow1Ok = (
        $probeValues["probe_fb_vram_word2"] -eq "0x9abc" -and
        $probeValues["probe_fb_vram_word3"] -eq "0xdef0"
      )
      Write-Output "probe_fb_tile_row0_vram=$fbTileRow0Ok expected=0x1234,0x5678 actual=$($probeValues["probe_fb_vram_word0"]),$($probeValues["probe_fb_vram_word1"])"
      Write-Output "probe_fb_tile_row1_vram=$fbTileRow1Ok expected=0x9abc,0xdef0 actual=$($probeValues["probe_fb_vram_word2"]),$($probeValues["probe_fb_vram_word3"])"
      if (-not $fbTileRow0Ok) {
        $failed += "probe_fb_vram_row0"
      }
      if (-not $fbTileRow1Ok) {
        $failed += "probe_fb_vram_row1"
      }
    } elseif ($Probe -eq "DualCpuWramSurvey" -or $Probe -eq "DualCpuWramSweep") {
      Write-Output "probe_sub_wram1_word0_observed=$($probeValues["probe_sub_result6"])"
      Write-Output "probe_main_wram_bank0_observed=$($probeValues["probe_wram_word0"]),$($probeValues["probe_wram_word1"])"
      Write-Output "probe_main_wram_bank1_observed=$($probeValues["probe_wram_bank1_word0"]),$($probeValues["probe_wram_bank1_word1"])"
      Write-Output "probe_main_wram_mcd1_bank0_observed=$($probeValues["probe_wram_mcd1_bank0_word0"]),$($probeValues["probe_wram_mcd1_bank0_word1"])"
      Write-Output "probe_main_wram_mcd1_bank1_observed=$($probeValues["probe_wram_mcd1_bank1_word0"]),$($probeValues["probe_wram_mcd1_bank1_word1"])"
    }
    Write-Output "probe_mem_mode_summary before=$($probeValues["probe_mem_mode_before"]) after_cmd=$($probeValues["probe_mem_mode_after"]) after_wram=$($probeValues["probe_mem_mode_after_wram"]) sub_snapshot=$($probeValues["probe_sub_result3"])"

    Write-Output "probe_gdb_exit=$gdbExit"
    Write-Output "probe_breakpoint_hit=$hitBreakpoint"

    if ($gdbExit -ne 0 -or -not $hitBreakpoint -or $failed.Count) {
      exit 1
    }
  } elseif ($Probe -eq "MegadevControl") {
    $joined = $gdbOutput -join "`n"
    $hitBreakpoint = ($joined -match "Breakpoint \d+, .*megadev_control_halt")
    $controlNames = @(
      "megadev_control_main_magic",
      "megadev_control_phase",
      "megadev_control_sub_flag",
      "megadev_control_stat0",
      "megadev_control_stat1",
      "megadev_control_stat2",
      "megadev_control_stat3",
      "megadev_control_trace"
    )
    $controlValues = Get-NamedProbeValues $gdbOutput $controlNames
    $expectedValues = [ordered]@{
      megadev_control_main_magic = "0x4d43"
      megadev_control_phase = "0x0002"
      megadev_control_stat0 = "0x5350"
      megadev_control_stat1 = "0x494e"
    }

    $failed = @()
    foreach ($name in $expectedValues.Keys) {
      $actualValue = $controlValues[$name]
      $ok = $actualValue -eq $expectedValues[$name]
      Write-Output "control_check_$name=$ok expected=$($expectedValues[$name]) actual=$actualValue"
      if (-not $ok) {
        $failed += $name
      }
    }

    $subFlag = $controlValues["megadev_control_sub_flag"]
    $subFlagOk = ($subFlag -eq "0x0003" -or $subFlag -eq "0x0005")
    Write-Output "control_check_sub_flag=$subFlagOk expected=0x0003_or_0x0005 actual=$subFlag"
    if (-not $subFlagOk) {
      $failed += "megadev_control_sub_flag"
    }

    Write-Output "control_stat2=$($controlValues["megadev_control_stat2"])"
    Write-Output "control_stat3=$($controlValues["megadev_control_stat3"])"
    Write-Output "control_trace=$($controlValues["megadev_control_trace"])"
    Write-Output "probe_gdb_exit=$gdbExit"
    Write-Output "probe_breakpoint_hit=$hitBreakpoint"

    if ($gdbExit -ne 0 -or -not $hitBreakpoint -or $failed.Count) {
      exit 1
    }
  } else {
    $expected = ($ExpectedPrefix -replace "[^0-9a-fA-F]", "").ToLowerInvariant()
    $actual = Get-HexBytesFromGdbLine $gdbOutput $IpAddress
    $hitBreakpoint = (($gdbOutput -join "`n") -match "Breakpoint \d+, 0x0*ff0000")
    $prefixOk = $actual.StartsWith($expected)

    Write-Output "probe_gdb_exit=$gdbExit"
    Write-Output "probe_breakpoint_hit=$hitBreakpoint"
    Write-Output "probe_expected_prefix=$expected"
    Write-Output "probe_actual_prefix=$actual"
    Write-Output "probe_prefix_ok=$prefixOk"

    if ($gdbExit -ne 0 -or -not $hitBreakpoint -or -not $prefixOk) {
      exit 1
    }
  }

  exit 0
}
finally {
  if ($proc -and -not $proc.HasExited) {
    Stop-Process -Id $proc.Id -Force
  }
}
