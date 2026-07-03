param(
  [string]$BlastEmDir = "C:\tmp\blastem\blastem-win64-0.6.3-pre-ec47c727cd65",
  [string]$Bios = "",
  [string]$Cue = "build\segaos.cue",
  [string]$Elf = "build\main_cpu.elf",
  [string]$Gdb = "C:\SDKS\SGDK\bin\gdb.exe",
  [int]$GdbTimeoutSeconds = 60,
  [string]$IpAddress = "0xff0000",
  [string]$ExpectedPrefix = "43fa000a4eb80364",
  [ValidateSet("Ip", "DualCpu", "DualCpuStatus", "DualCpuWramSurvey", "DualCpuWramRetClear", "DualCpuWramSweep", "Framebuffer", "MegadevControl", "RuntimeSmoke", "DesktopInit", "DesktopRepeat", "DesktopLoop", "DesktopTiming", "DesktopWm", "DesktopDirtyQueue", "DesktopScheduler", "DesktopPump", "BasicBram", "VdpText")]
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
    $_ -and
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
    $_ -and
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
        $_ -and
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

function Join-NativeArguments([string[]]$Arguments) {
  return (($Arguments | ForEach-Object {
    if ($_ -eq "") {
      '""'
    } elseif ($_ -match '[\s"]') {
      '"' + ($_ -replace '"', '\"') + '"'
    } else {
      $_
    }
  }) -join " ")
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

    $argumentLine = Join-NativeArguments $args
    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $gdbPath
    $startInfo.Arguments = $argumentLine
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $startInfo
    [void]$process.Start()

    $timer = [System.Diagnostics.Stopwatch]::StartNew()
    while (-not $process.HasExited -and
           $timer.Elapsed.TotalSeconds -lt $GdbTimeoutSeconds) {
      Start-Sleep -Milliseconds 100
      $process.Refresh()
    }

    $timedOut = -not $process.HasExited
    if ($timedOut) {
      try {
        $process.Kill()
      } catch {
      }
      try {
        $process.WaitForExit(5000) | Out-Null
      } catch {
      }
      $exitCode = 124
    } else {
      $process.WaitForExit()
      $process.Refresh()
      $exitCode = $process.ExitCode
    }

    $output = @()
    $stdoutText = ""
    $stderrText = ""
    try {
      $stdoutText = $process.StandardOutput.ReadToEnd()
    } catch {
    }
    try {
      $stderrText = $process.StandardError.ReadToEnd()
    } catch {
    }
    if ($stdoutText) {
      $output += ($stdoutText -split "`r?`n") | Where-Object { $_ -ne "" }
    }
    if ($stderrText) {
      $output += ($stderrText -split "`r?`n") | Where-Object { $_ -ne "" }
    }

    if ($timedOut) {
      $output += "probe_gdb_timeout=True"
      $output += "probe_gdb_timeout_seconds=$GdbTimeoutSeconds"
    } else {
      $output += "probe_gdb_timeout=False"
    }

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
  } elseif ($Probe -eq "RuntimeSmoke") {
    $smokeNames = @(
      "segaos_smoke_main_phase",
      "segaos_smoke_sub_flag",
      "segaos_smoke_stat0",
      "segaos_smoke_stat1",
      "segaos_smoke_stat2",
      "segaos_smoke_stat3",
      "segaos_smoke_trace",
      "segaos_smoke_done_status"
    )

    $gdbCommands += @(
      "break segaos_runtime_smoke_halt",
      "continue",
      "info registers pc sp"
    )
    foreach ($name in $smokeNames) {
      $gdbCommands += "echo $name="
      $gdbCommands += "p/x (unsigned short)$name"
    }
  } elseif ($Probe -eq "DesktopInit" -or $Probe -eq "DesktopRepeat" -or $Probe -eq "DesktopLoop" -or $Probe -eq "DesktopTiming" -or $Probe -eq "DesktopWm" -or $Probe -eq "DesktopDirtyQueue" -or $Probe -eq "DesktopScheduler" -or $Probe -eq "DesktopPump") {
    $desktopNames = @(
      "segaos_desktop_main_phase",
      "segaos_desktop_sub_flag",
      "segaos_desktop_stat0",
      "segaos_desktop_stat1",
      "segaos_desktop_stat2",
      "segaos_desktop_stat3",
      "segaos_desktop_stat5",
      "segaos_desktop_stat6",
      "segaos_desktop_trace",
      "segaos_desktop_main_flag",
      "segaos_desktop_ready_sub_flag",
      "segaos_desktop_ready_stat0",
      "segaos_desktop_ready_stat1",
      "segaos_desktop_ready_trace",
      "segaos_desktop_done_status",
      "segaos_desktop_render_status",
      "segaos_desktop_render_trace",
      "segaos_desktop_mem_mode_before",
      "segaos_desktop_mem_mode_after_return",
      "segaos_desktop_wait_polls",
      "segaos_desktop_text_probe_enabled",
      "segaos_desktop_text_wram_word0",
      "segaos_desktop_text_wram_word1",
      "segaos_desktop_text_vram_word0",
      "segaos_desktop_text_vram_word1",
      "segaos_desktop_text_wram_sig",
      "segaos_desktop_text_vram_sig",
      "segaos_desktop_text_plane_entry0",
      "segaos_desktop_text_plane_entry1",
      "segaos_desktop_text_plane_entry2",
      "segaos_desktop_title_probe_enabled",
      "segaos_desktop_title_wram_word0",
      "segaos_desktop_title_wram_word1",
      "segaos_desktop_title_vram_word0",
      "segaos_desktop_title_vram_word1"
    )
    if ($Probe -eq "DesktopScheduler") {
      $desktopNames = @(
        "segaos_desktop_main_phase",
        "segaos_desktop_stat0",
        "segaos_desktop_trace",
        "segaos_desktop_done_status",
        "segaos_desktop_wait_polls"
      )
    }
    if ($Probe -eq "DesktopRepeat") {
      $desktopNames += @(
        "segaos_desktop_repeat_status",
        "segaos_desktop_repeat_trace",
        "segaos_desktop_repeat_stat0",
        "segaos_desktop_repeat_sub_flag",
        "segaos_desktop_repeat_wait_polls",
        "segaos_desktop_repeat_mem_before",
        "segaos_desktop_repeat_mem_after_done",
        "segaos_desktop_repeat_mem_after_return",
        "segaos_desktop_repeat_title_vram_word0",
        "segaos_desktop_repeat_title_vram_word1"
      )
    }
    if ($Probe -eq "DesktopLoop") {
      $desktopNames += @(
        "segaos_desktop_loop_frame_count",
        "segaos_desktop_loop_status",
        "segaos_desktop_loop_trace",
        "segaos_desktop_loop_mem_before",
        "segaos_desktop_loop_mem_after_return",
        "segaos_desktop_loop_title_vram_word0",
        "segaos_desktop_loop_title_vram_word1"
      )
    }
    if ($Probe -eq "DesktopTiming") {
      $desktopNames += @(
        "segaos_desktop_timing_strip_count",
        "segaos_desktop_timing_hv_start",
        "segaos_desktop_timing_hv_end",
        "segaos_desktop_timing_status_end",
        "segaos_desktop_timing_transition_mask",
        "segaos_desktop_timing_dma_clear_mask"
      )
    }
    if ($Probe -eq "DesktopWm") {
      $desktopNames += @(
        "segaos_desktop_wait_last_status",
        "segaos_desktop_wait_first_non_idle",
        "segaos_desktop_wait_last_non_idle"
      )
    }
    if ($Probe -eq "DesktopDirtyQueue") {
      $desktopNames += @(
        "segaos_desktop_dirty_queue_result",
        "segaos_desktop_dirty_queue_count",
        "segaos_desktop_dirty_queue_bytes",
        "segaos_desktop_dirty_queue_first_tile",
        "segaos_desktop_dirty_queue_tile_count",
        "segaos_desktop_dirty_queue_wram_word0",
        "segaos_desktop_dirty_queue_wram_word1",
        "segaos_desktop_dirty_queue_vram_word0",
        "segaos_desktop_dirty_queue_vram_word1"
      )
    }
    if ($Probe -eq "DesktopScheduler") {
      $desktopNames += @(
        "segaos_desktop_scheduler_result",
        "segaos_desktop_scheduler_slice0_tile_count",
        "segaos_desktop_scheduler_slice0_next_tile",
        "segaos_desktop_scheduler_slice1_first_tile",
        "segaos_desktop_scheduler_slice1_tile_count",
        "segaos_desktop_scheduler_slice1_next_tile",
        "segaos_desktop_scheduler_slice1_wram_word",
        "segaos_desktop_scheduler_slice1_before_word",
        "segaos_desktop_scheduler_slice1_vram_word"
      )
    }
    if ($Probe -eq "DesktopPump") {
      $desktopNames = @(
        "segaos_desktop_main_phase",
        "segaos_desktop_stat0",
        "segaos_desktop_trace",
        "segaos_desktop_done_status",
        "segaos_desktop_wait_polls",
        "segaos_desktop_pump_result",
        "segaos_desktop_pump_frame_count",
        "segaos_desktop_pump_slice_count",
        "segaos_desktop_pump_final_first_tile",
        "segaos_desktop_pump_final_tile_count",
        "segaos_desktop_pump_mem_after_return"
      )
    }

    $gdbCommands += @(
      "break segaos_desktop_init_halt",
      "continue",
      "info registers pc sp"
    )
    foreach ($name in $desktopNames) {
      $gdbCommands += "echo $name="
      $gdbCommands += "p/x (unsigned short)$name"
    }
  } elseif ($Probe -eq "VdpText") {
    $vdpTextNames = @(
      "segaos_vdp_text_phase",
      "segaos_vdp_text_tile_s_index",
      "segaos_vdp_text_tile_t_index",
      "segaos_vdp_text_row1_word0",
      "segaos_vdp_text_row1_word1",
      "segaos_vdp_text_row2_word0",
      "segaos_vdp_text_row2_word1",
      "segaos_vdp_text_plane_entry0",
      "segaos_vdp_text_plane_entry1",
      "segaos_vdp_text_plane_entry2",
      "segaos_vdp_text_line1_entry0"
    )

    $gdbCommands += @(
      "break segaos_vdp_text_halt",
      "continue",
      "info registers pc sp"
    )
    foreach ($name in $vdpTextNames) {
      $gdbCommands += "echo $name="
      $gdbCommands += "p/x (unsigned short)$name"
    }
  } elseif ($Probe -eq "BasicBram") {
    $basicBramNames = @(
      "segaos_basic_bram_main_phase",
      "segaos_basic_bram_done_status",
      "segaos_basic_bram_result0",
      "segaos_basic_bram_result1",
      "segaos_basic_bram_result2",
      "segaos_basic_bram_result3",
      "segaos_basic_bram_result4",
      "segaos_basic_bram_result5",
      "segaos_basic_bram_result6",
      "segaos_basic_bram_result7"
    )

    $gdbCommands += @(
      "break segaos_basic_bram_probe_halt",
      "continue",
      "info registers pc sp"
    )
    foreach ($name in $basicBramNames) {
      $gdbCommands += "echo $name="
      $gdbCommands += "p/x (unsigned short)$name"
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
  } elseif ($Probe -eq "RuntimeSmoke") {
    $joined = $gdbOutput -join "`n"
    $hitBreakpoint = ($joined -match "Breakpoint \d+, .*segaos_runtime_smoke_halt")
    $smokeNames = @(
      "segaos_smoke_main_phase",
      "segaos_smoke_sub_flag",
      "segaos_smoke_stat0",
      "segaos_smoke_stat1",
      "segaos_smoke_stat2",
      "segaos_smoke_stat3",
      "segaos_smoke_trace",
      "segaos_smoke_done_status"
    )
    $smokeValues = Get-NamedProbeValues $gdbOutput $smokeNames
    $expectedValues = [ordered]@{
      segaos_smoke_main_phase = "0x80ff"
      segaos_smoke_sub_flag = "0x0000"
      segaos_smoke_stat0 = "0x534d"
      segaos_smoke_stat1 = "0x0003"
      segaos_smoke_trace = "0x72fe"
      segaos_smoke_done_status = "0x0003"
    }

    $failed = @()
    foreach ($name in $expectedValues.Keys) {
      $actualValue = $smokeValues[$name]
      $ok = $actualValue -eq $expectedValues[$name]
      Write-Output "smoke_check_$name=$ok expected=$($expectedValues[$name]) actual=$actualValue"
      if (-not $ok) {
        $failed += $name
      }
    }

    Write-Output "smoke_stat2=$($smokeValues["segaos_smoke_stat2"])"
    Write-Output "smoke_stat3=$($smokeValues["segaos_smoke_stat3"])"
    Write-Output "probe_gdb_exit=$gdbExit"
    Write-Output "probe_breakpoint_hit=$hitBreakpoint"

    if ($gdbExit -ne 0 -or -not $hitBreakpoint -or $failed.Count) {
      exit 1
    }
  } elseif ($Probe -eq "DesktopInit" -or $Probe -eq "DesktopRepeat" -or $Probe -eq "DesktopLoop" -or $Probe -eq "DesktopTiming" -or $Probe -eq "DesktopWm" -or $Probe -eq "DesktopDirtyQueue" -or $Probe -eq "DesktopScheduler" -or $Probe -eq "DesktopPump") {
    $joined = $gdbOutput -join "`n"
    $hitBreakpoint = ($joined -match "Breakpoint \d+, .*segaos_desktop_init_halt")
    $desktopNames = @(
      "segaos_desktop_main_phase",
      "segaos_desktop_sub_flag",
      "segaos_desktop_stat0",
      "segaos_desktop_stat1",
      "segaos_desktop_stat2",
      "segaos_desktop_stat3",
      "segaos_desktop_stat5",
      "segaos_desktop_stat6",
      "segaos_desktop_trace",
      "segaos_desktop_main_flag",
      "segaos_desktop_ready_sub_flag",
      "segaos_desktop_ready_stat0",
      "segaos_desktop_ready_stat1",
      "segaos_desktop_ready_trace",
      "segaos_desktop_done_status",
      "segaos_desktop_render_status",
      "segaos_desktop_render_trace",
      "segaos_desktop_mem_mode_before",
      "segaos_desktop_mem_mode_after_return",
      "segaos_desktop_wait_polls",
      "segaos_desktop_text_probe_enabled",
      "segaos_desktop_text_wram_word0",
      "segaos_desktop_text_wram_word1",
      "segaos_desktop_text_vram_word0",
      "segaos_desktop_text_vram_word1",
      "segaos_desktop_text_wram_sig",
      "segaos_desktop_text_vram_sig",
      "segaos_desktop_text_plane_entry0",
      "segaos_desktop_text_plane_entry1",
      "segaos_desktop_text_plane_entry2",
      "segaos_desktop_title_probe_enabled",
      "segaos_desktop_title_wram_word0",
      "segaos_desktop_title_wram_word1",
      "segaos_desktop_title_vram_word0",
      "segaos_desktop_title_vram_word1"
    )
    if ($Probe -eq "DesktopScheduler") {
      $desktopNames = @(
        "segaos_desktop_main_phase",
        "segaos_desktop_stat0",
        "segaos_desktop_trace",
        "segaos_desktop_done_status",
        "segaos_desktop_wait_polls",
        "segaos_desktop_scheduler_result",
        "segaos_desktop_scheduler_slice0_tile_count",
        "segaos_desktop_scheduler_slice0_next_tile",
        "segaos_desktop_scheduler_slice1_first_tile",
        "segaos_desktop_scheduler_slice1_tile_count",
        "segaos_desktop_scheduler_slice1_next_tile",
        "segaos_desktop_scheduler_slice1_wram_word",
        "segaos_desktop_scheduler_slice1_before_word",
        "segaos_desktop_scheduler_slice1_vram_word"
      )
    }
    if ($Probe -eq "DesktopPump") {
      $desktopNames = @(
        "segaos_desktop_main_phase",
        "segaos_desktop_stat0",
        "segaos_desktop_trace",
        "segaos_desktop_done_status",
        "segaos_desktop_wait_polls",
        "segaos_desktop_pump_result",
        "segaos_desktop_pump_frame_count",
        "segaos_desktop_pump_slice_count",
        "segaos_desktop_pump_final_first_tile",
        "segaos_desktop_pump_final_tile_count",
        "segaos_desktop_pump_mem_after_return"
      )
    }
    if ($Probe -eq "DesktopRepeat") {
      $desktopNames += @(
        "segaos_desktop_repeat_status",
        "segaos_desktop_repeat_trace",
        "segaos_desktop_repeat_stat0",
        "segaos_desktop_repeat_sub_flag",
        "segaos_desktop_repeat_wait_polls",
        "segaos_desktop_repeat_mem_before",
        "segaos_desktop_repeat_mem_after_done",
        "segaos_desktop_repeat_mem_after_return",
        "segaos_desktop_repeat_title_vram_word0",
        "segaos_desktop_repeat_title_vram_word1"
      )
    }
    if ($Probe -eq "DesktopLoop") {
      $desktopNames += @(
        "segaos_desktop_loop_frame_count",
        "segaos_desktop_loop_status",
        "segaos_desktop_loop_trace",
        "segaos_desktop_loop_mem_before",
        "segaos_desktop_loop_mem_after_return",
        "segaos_desktop_loop_title_vram_word0",
        "segaos_desktop_loop_title_vram_word1"
      )
    }
    if ($Probe -eq "DesktopTiming") {
      $desktopNames += @(
        "segaos_desktop_timing_strip_count",
        "segaos_desktop_timing_hv_start",
        "segaos_desktop_timing_hv_end",
        "segaos_desktop_timing_status_end",
        "segaos_desktop_timing_transition_mask",
        "segaos_desktop_timing_dma_clear_mask"
      )
    }
    if ($Probe -eq "DesktopWm") {
      $desktopNames += @(
        "segaos_desktop_wait_last_status",
        "segaos_desktop_wait_first_non_idle",
        "segaos_desktop_wait_last_non_idle"
      )
    }
    if ($Probe -eq "DesktopDirtyQueue") {
      $desktopNames += @(
        "segaos_desktop_dirty_queue_result",
        "segaos_desktop_dirty_queue_count",
        "segaos_desktop_dirty_queue_bytes",
        "segaos_desktop_dirty_queue_first_tile",
        "segaos_desktop_dirty_queue_tile_count",
        "segaos_desktop_dirty_queue_wram_word0",
        "segaos_desktop_dirty_queue_wram_word1",
        "segaos_desktop_dirty_queue_vram_word0",
        "segaos_desktop_dirty_queue_vram_word1"
      )
    }
    $desktopValues = Get-NamedProbeValues $gdbOutput $desktopNames
    $expectedValues = [ordered]@{
      segaos_desktop_main_phase = "0x81ff"
      segaos_desktop_sub_flag = "0x0000"
      segaos_desktop_stat0 = "0x0002"
      segaos_desktop_trace = "0x7404"
      segaos_desktop_done_status = "0x0003"
      segaos_desktop_render_status = "0x0003"
      segaos_desktop_render_trace = "0x7404"
    }
    if ($Probe -eq "DesktopRepeat") {
      $expectedValues["segaos_desktop_main_phase"] = "0x82ff"
      $expectedValues["segaos_desktop_repeat_status"] = "0x0003"
      $expectedValues["segaos_desktop_repeat_trace"] = "0x7404"
      $expectedValues["segaos_desktop_repeat_stat0"] = "0x0002"
      $expectedValues["segaos_desktop_repeat_sub_flag"] = "0x0000"
      $expectedValues["segaos_desktop_repeat_mem_before"] = "0x2a06"
      $expectedValues["segaos_desktop_repeat_mem_after_done"] = "0x2a06"
      $expectedValues["segaos_desktop_repeat_mem_after_return"] = "0x2a06"
      $expectedValues["segaos_desktop_repeat_title_vram_word0"] = "0xf11f"
      $expectedValues["segaos_desktop_repeat_title_vram_word1"] = "0x1f11"
    }
    if ($Probe -eq "DesktopLoop") {
      $expectedValues["segaos_desktop_main_phase"] = "0x83ff"
      $expectedValues["segaos_desktop_loop_frame_count"] = "0x0004"
      $expectedValues["segaos_desktop_loop_status"] = "0x0003"
      $expectedValues["segaos_desktop_loop_trace"] = "0x7404"
      $expectedValues["segaos_desktop_loop_mem_before"] = "0x2a06"
      $expectedValues["segaos_desktop_loop_mem_after_return"] = "0x2a06"
      $expectedValues["segaos_desktop_loop_title_vram_word0"] = "0xf11f"
      $expectedValues["segaos_desktop_loop_title_vram_word1"] = "0x1f11"
    }
    if ($Probe -eq "DesktopTiming") {
      $expectedValues["segaos_desktop_main_phase"] = "0x84ff"
      $expectedValues["segaos_desktop_timing_strip_count"] = "0x0007"
      $expectedValues["segaos_desktop_timing_transition_mask"] = "0x007f"
      $expectedValues["segaos_desktop_timing_dma_clear_mask"] = "0x007f"
    }
    if ($Probe -eq "DesktopWm") {
      $expectedValues["segaos_desktop_stat2"] = "0x0001"
      $expectedValues["segaos_desktop_stat3"] = "0x0007"
      $expectedValues["segaos_desktop_stat5"] = "0x2822"
    }
    if ($Probe -eq "DesktopDirtyQueue") {
      $expectedValues["segaos_desktop_main_phase"] = "0x85ff"
      $expectedValues["segaos_desktop_dirty_queue_result"] = "0x0001"
      $expectedValues["segaos_desktop_dirty_queue_count"] = "0x0001"
      $expectedValues["segaos_desktop_dirty_queue_bytes"] = "0x0020"
      $expectedValues["segaos_desktop_dirty_queue_first_tile"] = "0x0147"
      $expectedValues["segaos_desktop_dirty_queue_tile_count"] = "0x0001"
      $expectedValues["segaos_desktop_dirty_queue_wram_word0"] = "0xf11f"
      $expectedValues["segaos_desktop_dirty_queue_wram_word1"] = "0x1f11"
      $expectedValues["segaos_desktop_dirty_queue_vram_word0"] = "0xf11f"
      $expectedValues["segaos_desktop_dirty_queue_vram_word1"] = "0x1f11"
    }
    if ($Probe -eq "DesktopScheduler") {
      $expectedValues = [ordered]@{
        segaos_desktop_main_phase = "0x87ff"
        segaos_desktop_stat0 = "0x0002"
        segaos_desktop_trace = "0x7404"
        segaos_desktop_done_status = "0x0003"
      }
      $expectedValues["segaos_desktop_scheduler_result"] = "0x0001"
      $expectedValues["segaos_desktop_scheduler_slice0_tile_count"] = "0x00eb"
      $expectedValues["segaos_desktop_scheduler_slice0_next_tile"] = "0x00eb"
      $expectedValues["segaos_desktop_scheduler_slice1_first_tile"] = "0x00eb"
      $expectedValues["segaos_desktop_scheduler_slice1_tile_count"] = "0x00eb"
      $expectedValues["segaos_desktop_scheduler_slice1_next_tile"] = "0x01d6"
    }
    if ($Probe -eq "DesktopPump") {
      $expectedValues = [ordered]@{
        segaos_desktop_main_phase = "0x88ff"
        segaos_desktop_stat0 = "0x0002"
        segaos_desktop_trace = "0x7404"
        segaos_desktop_done_status = "0x0003"
        segaos_desktop_pump_result = "0x0001"
        segaos_desktop_pump_frame_count = "0x0004"
        segaos_desktop_pump_slice_count = "0x0005"
        segaos_desktop_pump_final_first_tile = "0x03ac"
        segaos_desktop_pump_final_tile_count = "0x00b4"
        segaos_desktop_pump_mem_after_return = "0x2a06"
      }
    }

    $failed = @()
    foreach ($name in $expectedValues.Keys) {
      $actualValue = $desktopValues[$name]
      $ok = $actualValue -eq $expectedValues[$name]
      Write-Output "desktop_check_$name=$ok expected=$($expectedValues[$name]) actual=$actualValue"
      if (-not $ok) {
        $failed += $name
      }
    }

    Write-Output "desktop_stat1=$($desktopValues["segaos_desktop_stat1"])"
    Write-Output "desktop_stat2=$($desktopValues["segaos_desktop_stat2"])"
    Write-Output "desktop_stat3=$($desktopValues["segaos_desktop_stat3"])"
    Write-Output "desktop_stat5=$($desktopValues["segaos_desktop_stat5"])"
    Write-Output "desktop_stat6=$($desktopValues["segaos_desktop_stat6"])"
    if ($Probe -eq "DesktopWm") {
      Write-Output "desktop_wm_window_count=$($desktopValues["segaos_desktop_stat2"])"
      Write-Output "desktop_wm_active_flags=$($desktopValues["segaos_desktop_stat3"])"
      Write-Output "desktop_wm_frame_left_top=$($desktopValues["segaos_desktop_stat5"])"
    }
    Write-Output "desktop_main_flag=$($desktopValues["segaos_desktop_main_flag"])"
    Write-Output "desktop_ready_sub_flag=$($desktopValues["segaos_desktop_ready_sub_flag"])"
    Write-Output "desktop_ready_stat0=$($desktopValues["segaos_desktop_ready_stat0"])"
    Write-Output "desktop_ready_stat1=$($desktopValues["segaos_desktop_ready_stat1"])"
    Write-Output "desktop_ready_trace=$($desktopValues["segaos_desktop_ready_trace"])"
    Write-Output "desktop_render_status=$($desktopValues["segaos_desktop_render_status"])"
    Write-Output "desktop_render_trace=$($desktopValues["segaos_desktop_render_trace"])"
    Write-Output "desktop_mem_mode_before=$($desktopValues["segaos_desktop_mem_mode_before"])"
    Write-Output "desktop_mem_mode_after_return=$($desktopValues["segaos_desktop_mem_mode_after_return"])"
    Write-Output "desktop_wait_polls=$($desktopValues["segaos_desktop_wait_polls"])"
    if ($Probe -eq "DesktopWm") {
      Write-Output "desktop_wait_last_status=$($desktopValues["segaos_desktop_wait_last_status"])"
      Write-Output "desktop_wait_first_non_idle=$($desktopValues["segaos_desktop_wait_first_non_idle"])"
      Write-Output "desktop_wait_last_non_idle=$($desktopValues["segaos_desktop_wait_last_non_idle"])"
    }
    Write-Output "desktop_text_probe_enabled=$($desktopValues["segaos_desktop_text_probe_enabled"])"
    Write-Output "desktop_text_wram=$($desktopValues["segaos_desktop_text_wram_word0"]),$($desktopValues["segaos_desktop_text_wram_word1"])"
    Write-Output "desktop_text_vram=$($desktopValues["segaos_desktop_text_vram_word0"]),$($desktopValues["segaos_desktop_text_vram_word1"])"
    Write-Output "desktop_text_sig wram=$($desktopValues["segaos_desktop_text_wram_sig"]) vram=$($desktopValues["segaos_desktop_text_vram_sig"])"
    Write-Output "desktop_text_plane=$($desktopValues["segaos_desktop_text_plane_entry0"]),$($desktopValues["segaos_desktop_text_plane_entry1"]),$($desktopValues["segaos_desktop_text_plane_entry2"])"
    Write-Output "desktop_title_probe_enabled=$($desktopValues["segaos_desktop_title_probe_enabled"])"
    Write-Output "desktop_title_wram=$($desktopValues["segaos_desktop_title_wram_word0"]),$($desktopValues["segaos_desktop_title_wram_word1"])"
    Write-Output "desktop_title_vram=$($desktopValues["segaos_desktop_title_vram_word0"]),$($desktopValues["segaos_desktop_title_vram_word1"])"
    if ($Probe -eq "DesktopRepeat") {
      Write-Output "desktop_repeat_status=$($desktopValues["segaos_desktop_repeat_status"])"
      Write-Output "desktop_repeat_trace=$($desktopValues["segaos_desktop_repeat_trace"])"
      Write-Output "desktop_repeat_stat0=$($desktopValues["segaos_desktop_repeat_stat0"])"
      Write-Output "desktop_repeat_sub_flag=$($desktopValues["segaos_desktop_repeat_sub_flag"])"
      Write-Output "desktop_repeat_wait_polls=$($desktopValues["segaos_desktop_repeat_wait_polls"])"
      Write-Output "desktop_repeat_mem before=$($desktopValues["segaos_desktop_repeat_mem_before"]) after_done=$($desktopValues["segaos_desktop_repeat_mem_after_done"]) after_return=$($desktopValues["segaos_desktop_repeat_mem_after_return"])"
      Write-Output "desktop_repeat_title_vram=$($desktopValues["segaos_desktop_repeat_title_vram_word0"]),$($desktopValues["segaos_desktop_repeat_title_vram_word1"])"
    }
    if ($Probe -eq "DesktopLoop") {
      Write-Output "desktop_loop_frame_count=$($desktopValues["segaos_desktop_loop_frame_count"])"
      Write-Output "desktop_loop_status=$($desktopValues["segaos_desktop_loop_status"])"
      Write-Output "desktop_loop_trace=$($desktopValues["segaos_desktop_loop_trace"])"
      Write-Output "desktop_loop_mem before=$($desktopValues["segaos_desktop_loop_mem_before"]) after_return=$($desktopValues["segaos_desktop_loop_mem_after_return"])"
      Write-Output "desktop_loop_title_vram=$($desktopValues["segaos_desktop_loop_title_vram_word0"]),$($desktopValues["segaos_desktop_loop_title_vram_word1"])"
    }
    if ($Probe -eq "DesktopTiming") {
      Write-Output "desktop_timing_strip_count=$($desktopValues["segaos_desktop_timing_strip_count"])"
      Write-Output "desktop_timing_hv start=$($desktopValues["segaos_desktop_timing_hv_start"]) end=$($desktopValues["segaos_desktop_timing_hv_end"])"
      Write-Output "desktop_timing_status end=$($desktopValues["segaos_desktop_timing_status_end"])"
      Write-Output "desktop_timing_masks transition=$($desktopValues["segaos_desktop_timing_transition_mask"]) dma_clear=$($desktopValues["segaos_desktop_timing_dma_clear_mask"])"
    }
    if ($Probe -eq "DesktopDirtyQueue") {
      Write-Output "desktop_dirty_queue result=$($desktopValues["segaos_desktop_dirty_queue_result"]) count=$($desktopValues["segaos_desktop_dirty_queue_count"]) bytes=$($desktopValues["segaos_desktop_dirty_queue_bytes"])"
      Write-Output "desktop_dirty_queue_tile first=$($desktopValues["segaos_desktop_dirty_queue_first_tile"]) count=$($desktopValues["segaos_desktop_dirty_queue_tile_count"])"
      Write-Output "desktop_dirty_queue_wram=$($desktopValues["segaos_desktop_dirty_queue_wram_word0"]),$($desktopValues["segaos_desktop_dirty_queue_wram_word1"])"
      Write-Output "desktop_dirty_queue_vram=$($desktopValues["segaos_desktop_dirty_queue_vram_word0"]),$($desktopValues["segaos_desktop_dirty_queue_vram_word1"])"
    }
    if ($Probe -eq "DesktopScheduler") {
      Write-Output "desktop_scheduler result=$($desktopValues["segaos_desktop_scheduler_result"])"
      Write-Output "desktop_scheduler_slice0 tiles=$($desktopValues["segaos_desktop_scheduler_slice0_tile_count"]) next=$($desktopValues["segaos_desktop_scheduler_slice0_next_tile"])"
      Write-Output "desktop_scheduler_slice1 first=$($desktopValues["segaos_desktop_scheduler_slice1_first_tile"]) tiles=$($desktopValues["segaos_desktop_scheduler_slice1_tile_count"]) next=$($desktopValues["segaos_desktop_scheduler_slice1_next_tile"])"
      Write-Output "desktop_scheduler_slice1_words wram=$($desktopValues["segaos_desktop_scheduler_slice1_wram_word"]) before=$($desktopValues["segaos_desktop_scheduler_slice1_before_word"]) vram=$($desktopValues["segaos_desktop_scheduler_slice1_vram_word"])"
      $schedulerSlice1Ok = (
        $desktopValues["segaos_desktop_scheduler_slice1_wram_word"] -eq $desktopValues["segaos_desktop_scheduler_slice1_vram_word"] -and
        $desktopValues["segaos_desktop_scheduler_slice1_before_word"] -ne $desktopValues["segaos_desktop_scheduler_slice1_wram_word"]
      )
      Write-Output "desktop_scheduler_slice1_word_check=$schedulerSlice1Ok"
      if (-not $schedulerSlice1Ok) {
        $failed += "segaos_desktop_scheduler_slice1_word"
      }
    }
    if ($Probe -eq "DesktopPump") {
      Write-Output "desktop_pump result=$($desktopValues["segaos_desktop_pump_result"]) frames=$($desktopValues["segaos_desktop_pump_frame_count"]) slices=$($desktopValues["segaos_desktop_pump_slice_count"])"
      Write-Output "desktop_pump_final first=$($desktopValues["segaos_desktop_pump_final_first_tile"]) tiles=$($desktopValues["segaos_desktop_pump_final_tile_count"])"
      Write-Output "desktop_pump_return mem_after=$($desktopValues["segaos_desktop_pump_mem_after_return"])"
      Write-Output "desktop_pump_second status=$($desktopValues["segaos_desktop_done_status"]) trace=$($desktopValues["segaos_desktop_trace"]) stat0=$($desktopValues["segaos_desktop_stat0"])"
    }

    if ($desktopValues["segaos_desktop_text_probe_enabled"] -eq "0x0001") {
      $textWramOk = (
        $desktopValues["segaos_desktop_text_wram_word0"] -eq "0xffff" -and
        $desktopValues["segaos_desktop_text_wram_word1"] -eq "0xff11"
      )
      $textVramOk = (
        $desktopValues["segaos_desktop_text_vram_word0"] -eq "0xffff" -and
        $desktopValues["segaos_desktop_text_vram_word1"] -eq "0xff11"
      )
      Write-Output "desktop_text_wram_check=$textWramOk expected=0xffff,0xff11 actual=$($desktopValues["segaos_desktop_text_wram_word0"]),$($desktopValues["segaos_desktop_text_wram_word1"])"
      Write-Output "desktop_text_vram_check=$textVramOk expected=0xffff,0xff11 actual=$($desktopValues["segaos_desktop_text_vram_word0"]),$($desktopValues["segaos_desktop_text_vram_word1"])"
      $textWramGlyphOk = ($desktopValues["segaos_desktop_text_wram_sig"] -eq "0xd2dd")
      $textVramGlyphOk = ($desktopValues["segaos_desktop_text_vram_sig"] -eq "0xd2dd")
      $textPlaneOk = (
        $desktopValues["segaos_desktop_text_plane_entry0"] -eq "0x0198" -and
        $desktopValues["segaos_desktop_text_plane_entry1"] -eq "0x0199" -and
        $desktopValues["segaos_desktop_text_plane_entry2"] -eq "0x019a"
      )
      Write-Output "desktop_text_wram_glyph_check=$textWramGlyphOk expected=0xd2dd actual=$($desktopValues["segaos_desktop_text_wram_sig"])"
      Write-Output "desktop_text_vram_glyph_check=$textVramGlyphOk expected=0xd2dd actual=$($desktopValues["segaos_desktop_text_vram_sig"])"
      Write-Output "desktop_text_plane_check=$textPlaneOk expected=0x0198,0x0199,0x019a actual=$($desktopValues["segaos_desktop_text_plane_entry0"]),$($desktopValues["segaos_desktop_text_plane_entry1"]),$($desktopValues["segaos_desktop_text_plane_entry2"])"
      if (-not $textWramOk) {
        $failed += "segaos_desktop_text_wram"
      }
      if (-not $textVramOk) {
        $failed += "segaos_desktop_text_vram"
      }
      if (-not $textWramGlyphOk) {
        $failed += "segaos_desktop_text_wram_glyph"
      }
      if (-not $textVramGlyphOk) {
        $failed += "segaos_desktop_text_vram_glyph"
      }
      if (-not $textPlaneOk) {
        $failed += "segaos_desktop_text_plane"
      }
    }

    if ($desktopValues["segaos_desktop_title_probe_enabled"] -eq "0x0001") {
      $titleWramOk = (
        $desktopValues["segaos_desktop_title_wram_word0"] -eq "0xf11f" -and
        $desktopValues["segaos_desktop_title_wram_word1"] -eq "0x1f11"
      )
      $titleVramOk = (
        $desktopValues["segaos_desktop_title_vram_word0"] -eq "0xf11f" -and
        $desktopValues["segaos_desktop_title_vram_word1"] -eq "0x1f11"
      )
      Write-Output "desktop_title_wram_check=$titleWramOk expected=0xf11f,0x1f11 actual=$($desktopValues["segaos_desktop_title_wram_word0"]),$($desktopValues["segaos_desktop_title_wram_word1"])"
      Write-Output "desktop_title_vram_check=$titleVramOk expected=0xf11f,0x1f11 actual=$($desktopValues["segaos_desktop_title_vram_word0"]),$($desktopValues["segaos_desktop_title_vram_word1"])"
      if (-not $titleWramOk) {
        $failed += "segaos_desktop_title_wram"
      }
      if (-not $titleVramOk) {
        $failed += "segaos_desktop_title_vram"
      }
    }
    Write-Output "probe_gdb_exit=$gdbExit"
    Write-Output "probe_breakpoint_hit=$hitBreakpoint"

    if ($gdbExit -ne 0 -or -not $hitBreakpoint -or $failed.Count) {
      exit 1
    }
  } elseif ($Probe -eq "VdpText") {
    $joined = $gdbOutput -join "`n"
    $hitBreakpoint = ($joined -match "Breakpoint \d+, .*segaos_vdp_text_halt")
    $vdpTextNames = @(
      "segaos_vdp_text_phase",
      "segaos_vdp_text_tile_s_index",
      "segaos_vdp_text_tile_t_index",
      "segaos_vdp_text_row1_word0",
      "segaos_vdp_text_row1_word1",
      "segaos_vdp_text_row2_word0",
      "segaos_vdp_text_row2_word1",
      "segaos_vdp_text_plane_entry0",
      "segaos_vdp_text_plane_entry1",
      "segaos_vdp_text_plane_entry2",
      "segaos_vdp_text_line1_entry0"
    )
    $vdpTextValues = Get-NamedProbeValues $gdbOutput $vdpTextNames
    $expectedValues = [ordered]@{
      segaos_vdp_text_phase = "0x70ff"
      segaos_vdp_text_tile_s_index = "0x0001"
      segaos_vdp_text_tile_t_index = "0x0006"
      segaos_vdp_text_row1_word0 = "0x00ff"
      segaos_vdp_text_row1_word1 = "0xff00"
      segaos_vdp_text_row2_word0 = "0x0ff0"
      segaos_vdp_text_row2_word1 = "0x0000"
      segaos_vdp_text_plane_entry0 = "0x0001"
      segaos_vdp_text_plane_entry1 = "0x0002"
      segaos_vdp_text_plane_entry2 = "0x0003"
      segaos_vdp_text_line1_entry0 = "0x0006"
    }

    $failed = @()
    foreach ($name in $expectedValues.Keys) {
      $actualValue = $vdpTextValues[$name]
      $ok = $actualValue -eq $expectedValues[$name]
      Write-Output "vdp_text_check_$name=$ok expected=$($expectedValues[$name]) actual=$actualValue"
      if (-not $ok) {
        $failed += $name
      }
    }

    Write-Output "probe_gdb_exit=$gdbExit"
    Write-Output "probe_breakpoint_hit=$hitBreakpoint"

    if ($gdbExit -ne 0 -or -not $hitBreakpoint -or $failed.Count) {
      exit 1
    }
  } elseif ($Probe -eq "BasicBram") {
    $joined = $gdbOutput -join "`n"
    $hitBreakpoint = ($joined -match "Breakpoint \d+, .*segaos_basic_bram_probe_halt")
    $basicBramNames = @(
      "segaos_basic_bram_main_phase",
      "segaos_basic_bram_done_status",
      "segaos_basic_bram_result0",
      "segaos_basic_bram_result1",
      "segaos_basic_bram_result2",
      "segaos_basic_bram_result3",
      "segaos_basic_bram_result4",
      "segaos_basic_bram_result5",
      "segaos_basic_bram_result6",
      "segaos_basic_bram_result7"
    )
    $basicBramValues = Get-NamedProbeValues $gdbOutput $basicBramNames
    $expectedValues = [ordered]@{
      segaos_basic_bram_main_phase = "0x86ff"
      segaos_basic_bram_done_status = "0x0003"
      segaos_basic_bram_result0 = "0x5342"
      segaos_basic_bram_result1 = "0x0000"
      segaos_basic_bram_result2 = "0x0003"
      segaos_basic_bram_result5 = "0x0101"
      segaos_basic_bram_result6 = "0x0211"
      segaos_basic_bram_result7 = "0x75ff"
    }

    $failed = @()
    foreach ($name in $expectedValues.Keys) {
      $actualValue = $basicBramValues[$name]
      $ok = $actualValue -eq $expectedValues[$name]
      Write-Output "basic_bram_check_$name=$ok expected=$($expectedValues[$name]) actual=$actualValue"
      if (-not $ok) {
        $failed += $name
      }
    }

    Write-Output "basic_bram_total_blocks4k=$($basicBramValues["segaos_basic_bram_result3"])"
    Write-Output "basic_bram_free_blocks4k=$($basicBramValues["segaos_basic_bram_result4"])"
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
