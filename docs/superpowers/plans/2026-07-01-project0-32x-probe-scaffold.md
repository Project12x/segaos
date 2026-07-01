# Project 0 32X Probe Scaffold Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a non-invasive Project 0 scaffold that records 32X clean-room provenance, checks for a 32X toolchain, and defines a host-tested result schema before any 32X boot ROM work begins.

**Architecture:** Keep the current Sega CD build path untouched except for one host-test addition. Put 32X research assets under `probes/32x_project0/`, keep long-lived research notes under `docs/reference/`, and use `tools/check_32x_project0_env.ps1` to make missing SH-2 tooling explicit. The first executable contract is a C99 result-record schema that later 68K/SH-2 probes can write and host tools can parse.

**Tech Stack:** Existing Windows PowerShell, existing `Makefile` host-test flow, C99 host compiler via `HOST_CC`, Markdown docs, and external 32X tooling discovered through `SEGA32X_M68K_BIN`, `SEGA32X_SH2_BIN`, or common 32XDK/gendev paths.

---

## Scope Check

This plan implements the Project 0 Phase 0 scaffold only. It does not build a
32X ROM and it does not add SH-2 source yet, because the local machine currently
has SGDK under `C:\SDKS\SGDK` but no visible 32X SH-2 toolchain under
`C:\SDKS`. The actual 32X boot/mailbox ROM should be planned after this
scaffold records the 32XDK/manual provenance and the toolchain check passes.

## File Structure

- Create `tools/check_32x_project0_env.ps1`
  - Detects the M68K and SH-2 toolchain binaries.
  - Prints exact environment variables to set when detection fails.
  - Exits `0` only when both toolchains are usable.
- Create `probes/32x_project0/README.md`
  - Explains what Project 0 Phase 0 is and how to run the checker and host test.
  - States that no GPL/LinuxMD source is copied into SegaOS probes.
- Create `probes/32x_project0/project0_probe.h`
  - Defines the stable result record shared by later probe code and host tools.
  - Uses fixed-width integer fields and explicit status/probe IDs.
- Create `tests/test_project0_probe_layout.c`
  - Host test for the result schema constants, size, offsets, and initializer.
- Create `docs/reference/32x_project0_reference_pass.md`
  - Records current source/toolchain references and the next source files to
    inspect before 32X boot code is written.
- Modify `docs/reference/README.md`
  - Adds the new reference pass to the index.
- Modify `Makefile`
  - Adds the new host test to `host-tests`.

## Task 1: Add The 32X Project 0 Environment Checker

**Files:**
- Create: `tools/check_32x_project0_env.ps1`

- [ ] **Step 1: Create the script**

Add this full file:

```powershell
param(
  [string]$M68kBin = $env:SEGA32X_M68K_BIN,
  [string]$Sh2Bin = $env:SEGA32X_SH2_BIN
)

$ErrorActionPreference = "Stop"

function Resolve-BinDir([string]$Explicit, [string[]]$Candidates) {
  if ($Explicit) {
    $resolved = Resolve-Path -LiteralPath $Explicit -ErrorAction SilentlyContinue
    if ($resolved) {
      return $resolved.Path
    }
    return ""
  }

  foreach ($candidate in $Candidates) {
    $resolved = Resolve-Path -LiteralPath $candidate -ErrorAction SilentlyContinue
    if ($resolved) {
      return $resolved.Path
    }
  }

  return ""
}

function Find-Tool([string]$BinDir, [string[]]$Names) {
  if (-not $BinDir) {
    return ""
  }

  foreach ($name in $Names) {
    $candidate = Join-Path $BinDir $name
    $resolved = Resolve-Path -LiteralPath $candidate -ErrorAction SilentlyContinue
    if ($resolved) {
      return $resolved.Path
    }
  }

  return ""
}

$commonM68kBins = @(
  "C:\SDKS\32XDK\bin",
  "C:\SDKS\gendev\bin",
  "C:\SDKS\SGDK\bin"
)
$commonSh2Bins = @(
  "C:\SDKS\32XDK\bin",
  "C:\SDKS\gendev\bin"
)

$m68kBinDir = Resolve-BinDir $M68kBin $commonM68kBins
$sh2BinDir = Resolve-BinDir $Sh2Bin $commonSh2Bins

$m68kGcc = Find-Tool $m68kBinDir @("m68k-elf-gcc.exe", "m68k-elf-gcc", "gcc.exe")
$m68kObjcopy = Find-Tool $m68kBinDir @("m68k-elf-objcopy.exe", "m68k-elf-objcopy", "objcopy.exe")
$sh2Gcc = Find-Tool $sh2BinDir @("sh-elf-gcc.exe", "sh-elf-gcc", "sh2-elf-gcc.exe", "sh2-elf-gcc")
$sh2Objcopy = Find-Tool $sh2BinDir @("sh-elf-objcopy.exe", "sh-elf-objcopy", "sh2-elf-objcopy.exe", "sh2-elf-objcopy")

Write-Output "project0_m68k_bin=$m68kBinDir"
Write-Output "project0_sh2_bin=$sh2BinDir"
Write-Output "project0_m68k_gcc=$m68kGcc"
Write-Output "project0_m68k_objcopy=$m68kObjcopy"
Write-Output "project0_sh2_gcc=$sh2Gcc"
Write-Output "project0_sh2_objcopy=$sh2Objcopy"

$missing = @()
if (-not $m68kGcc) { $missing += "m68k gcc" }
if (-not $m68kObjcopy) { $missing += "m68k objcopy" }
if (-not $sh2Gcc) { $missing += "sh2 gcc" }
if (-not $sh2Objcopy) { $missing += "sh2 objcopy" }

if ($missing.Count -gt 0) {
  Write-Output "project0_env_ok=False"
  Write-Output "project0_missing=$($missing -join ', ')"
  Write-Output "Set SEGA32X_M68K_BIN to the directory containing the M68K tools."
  Write-Output "Set SEGA32X_SH2_BIN to the directory containing the SH-2 tools."
  exit 1
}

Write-Output "project0_env_ok=True"
exit 0
```

- [ ] **Step 2: Run the checker in the current environment**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_32x_project0_env.ps1
```

Expected on the current machine before installing 32X tooling:

```text
project0_env_ok=False
```

The command may exit `1`. That is the correct result when no SH-2 toolchain is
installed.

- [ ] **Step 3: Commit the checker**

Run:

```powershell
git add tools/check_32x_project0_env.ps1
git commit -m "tools: add 32x project0 environment check"
```

## Task 2: Add The Project 0 Probe Result Schema

**Files:**
- Create: `probes/32x_project0/project0_probe.h`
- Create: `tests/test_project0_probe_layout.c`
- Modify: `Makefile`

- [ ] **Step 1: Create the probe directory and result header**

Create `probes/32x_project0/project0_probe.h`:

```c
#ifndef PROJECT0_PROBE_H
#define PROJECT0_PROBE_H

#include <stdint.h>

#define P0_PROBE_MAGIC 0x5030u
#define P0_RESULT_VERSION 0x0001u

enum {
  P0_PROBE_BOOT_MAILBOX = 0x0001u,
  P0_PROBE_FRAMEBUFFER_THROUGHPUT = 0x0002u,
  P0_PROBE_ED_RAM_VISIBILITY = 0x0003u,
  P0_PROBE_ED_SERVICE_VISIBILITY = 0x0004u,
  P0_PROBE_CD_BRIDGE = 0x0005u,
  P0_PROBE_WRITABLE_STORAGE = 0x0006u
};

enum {
  P0_STATUS_NOT_RUN = 0x0000u,
  P0_STATUS_PASS = 0x0001u,
  P0_STATUS_FAIL = 0x0002u,
  P0_STATUS_UNSUPPORTED = 0x0003u,
  P0_STATUS_UNSAFE = 0x0004u
};

typedef struct P0_Result {
  uint16_t magic;
  uint16_t version;
  uint16_t probe_id;
  uint16_t status;
  uint32_t frames_elapsed;
  uint32_t ticks_elapsed;
  uint16_t value0;
  uint16_t value1;
  uint16_t value2;
  uint16_t value3;
  uint32_t address0;
  uint32_t address1;
} P0_Result;

#define P0_RESULT_INIT(id)                                                   \
  {                                                                          \
    P0_PROBE_MAGIC, P0_RESULT_VERSION, (uint16_t)(id), P0_STATUS_NOT_RUN,    \
        0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u                                       \
  }

#endif
```

- [ ] **Step 2: Create the host layout test**

Create `tests/test_project0_probe_layout.c`:

```c
#include <stddef.h>
#include <stdio.h>

#include "project0_probe.h"

#define CHECK_TRUE(expr)                                                      \
  do {                                                                        \
    if (!(expr)) {                                                            \
      printf("CHECK_TRUE failed: %s at line %d\n", #expr, __LINE__);          \
      return __LINE__;                                                        \
    }                                                                         \
  } while (0)

int main(void) {
  P0_Result result = P0_RESULT_INIT(P0_PROBE_BOOT_MAILBOX);

  CHECK_TRUE(P0_PROBE_MAGIC == 0x5030u);
  CHECK_TRUE(P0_RESULT_VERSION == 0x0001u);
  CHECK_TRUE(P0_PROBE_WRITABLE_STORAGE == 0x0006u);
  CHECK_TRUE(P0_STATUS_UNSAFE == 0x0004u);

  CHECK_TRUE(sizeof(P0_Result) == 32u);
  CHECK_TRUE(offsetof(P0_Result, magic) == 0u);
  CHECK_TRUE(offsetof(P0_Result, version) == 2u);
  CHECK_TRUE(offsetof(P0_Result, probe_id) == 4u);
  CHECK_TRUE(offsetof(P0_Result, status) == 6u);
  CHECK_TRUE(offsetof(P0_Result, frames_elapsed) == 8u);
  CHECK_TRUE(offsetof(P0_Result, ticks_elapsed) == 12u);
  CHECK_TRUE(offsetof(P0_Result, value0) == 16u);
  CHECK_TRUE(offsetof(P0_Result, value3) == 22u);
  CHECK_TRUE(offsetof(P0_Result, address0) == 24u);
  CHECK_TRUE(offsetof(P0_Result, address1) == 28u);

  CHECK_TRUE(result.magic == P0_PROBE_MAGIC);
  CHECK_TRUE(result.version == P0_RESULT_VERSION);
  CHECK_TRUE(result.probe_id == P0_PROBE_BOOT_MAILBOX);
  CHECK_TRUE(result.status == P0_STATUS_NOT_RUN);
  CHECK_TRUE(result.frames_elapsed == 0u);
  CHECK_TRUE(result.address1 == 0u);

  return 0;
}
```

- [ ] **Step 3: Wire the host test into `Makefile`**

In the `host-tests` target, after the storage policy test and before the
PowerShell timeout test, add:

```make
	$(HOST_CC) -std=c99 -Wall -Wextra -Iinclude -Iprobes/32x_project0 tests/test_project0_probe_layout.c -o $(BUILD_DIR)/test_project0_probe_layout.exe
	$(BUILD_DIR)/test_project0_probe_layout.exe
```

- [ ] **Step 4: Run the new host test directly**

Run:

```powershell
C:\SDKS\SGDK\bin\make.exe -r -f Makefile host-tests
```

Expected:

```text
test_project0_probe_layout.exe
```

The full `host-tests` target should exit `0`.

- [ ] **Step 5: Commit the schema and test**

Run:

```powershell
git add Makefile probes/32x_project0/project0_probe.h tests/test_project0_probe_layout.c
git commit -m "test: add 32x project0 result schema"
```

## Task 3: Add The Project 0 Runbook

**Files:**
- Create: `probes/32x_project0/README.md`

- [ ] **Step 1: Create the runbook**

Create `probes/32x_project0/README.md`:

```markdown
# Project 0 32X Probe Scaffold

Project 0 answers whether a Mega Drive + Sega CD + 32X + Mega EverDrive stack
is worth pursuing for SegaOS-Mars or Linux32X-SH2.

This directory is intentionally separate from the Sega CD boot-disc build. The
current phase defines tooling checks and result-record contracts only. It does
not build a 32X ROM.

## Clean-Room Boundary

- SegaOS probes may use public hardware behavior, independently written code,
  and permissively licensed code only after provenance is recorded.
- GPL LinuxMD source may inform Linux-side experiments, but it must not be
  copied or closely ported into this SegaOS probe tree.
- 32XDK and 32X hardware manual usage must be recorded in
  `docs/reference/32x_project0_reference_pass.md` before boot code is written.

## Environment Check

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_32x_project0_env.ps1
```

If the checker fails, install or point to a 32X-capable toolchain and set:

```powershell
$env:SEGA32X_M68K_BIN = "C:\path\to\m68k\bin"
$env:SEGA32X_SH2_BIN = "C:\path\to\sh2\bin"
```

## Host Contract Check

Run:

```powershell
C:\SDKS\SGDK\bin\make.exe -r -f Makefile host-tests
```

The host tests include `tests/test_project0_probe_layout.c`, which proves the
binary layout of `P0_Result`.

## Probe Order

1. 32X boot and 68K to SH-2 mailbox.
2. SH-2 framebuffer throughput.
3. SH-2 visibility into cartridge or Mega EverDrive RAM.
4. SH-2 visibility into Mega EverDrive service registers.
5. 68K to SH-2 to Sega CD service bridge.
6. Writable storage behavior.

## Result Record

Each future probe should write one `P0_Result` with:

- `magic = 0x5030`
- `version = 1`
- `probe_id` from `project0_probe.h`
- `status` from `project0_probe.h`
- frame/tick counters for timing
- value and address fields for probe-specific evidence
```

- [ ] **Step 2: Commit the runbook**

Run:

```powershell
git add probes/32x_project0/README.md
git commit -m "docs: add 32x project0 runbook"
```

## Task 4: Add The 32X Reference Pass Note

**Files:**
- Create: `docs/reference/32x_project0_reference_pass.md`
- Modify: `docs/reference/README.md`

- [ ] **Step 1: Create the reference note**

Create `docs/reference/32x_project0_reference_pass.md`:

```markdown
# 32X Project 0 Reference Pass

Date: 2026-07-01

This note records the source and documentation references required before
Project 0 writes 32X boot/mailbox code.

## Current Local Tooling Observation

On 2026-07-01, `C:\SDKS` contains SGDK but no visible 32X SH-2 toolchain
directory. Project 0 therefore starts with environment detection and a
host-tested result schema.

## Public References Checked

| Reference | URL | Date Checked | Licensing / Use |
|---|---|---|---|
| 32XDK repository | https://github.com/viciious/32XDK | 2026-07-01 | Toolchain and documentation reference. Inspect license and files before copying or closely porting source. |
| 32XDK releases | https://github.com/viciious/32XDK/releases | 2026-07-01 | Latest visible release is `20220418 devkit`, commit `25d2925`, listing gcc 12.1.0, binutils 2.38, newlib 4.2.0, and zasm 4.4. |
| Sega 32X hardware manual link from 32XDK README | http://www.tmeeco.eu/SMD/32x_hardware_manual.pdf | 2026-07-01 | Hardware behavior reference. Do not copy prose or diagrams into SegaOS docs. |

## Required File-Level Inspection Before 32X Boot Code

Before implementing a 32X boot ROM or SH-2 startup sequence:

1. Record the exact 32XDK commit or release archive hash used.
2. Record the license file or absence of one.
3. Inspect only the files needed for build invocation and boot layout.
4. Record the inspected paths and reuse mode.
5. Use `pattern-only` unless the license is explicitly permissive and the file
   is small enough to justify direct reuse with attribution.

## Initial Reuse Decision

The Phase 0 scaffold uses clean-room code only:

- `tools/check_32x_project0_env.ps1` is original project tooling.
- `probes/32x_project0/project0_probe.h` is an original result schema.
- `tests/test_project0_probe_layout.c` is an original host test.

No 32XDK, LinuxMD, Linux kernel, U-Boot, or emulator source is copied or closely
ported by the Phase 0 scaffold.
```

- [ ] **Step 2: Add the reference note to the index**

In `docs/reference/README.md`, under `### Architecture & Design`, add this
line after `full_stack_cleanroom_research.md`:

```markdown
- [32x_project0_reference_pass.md](32x_project0_reference_pass.md) — Toolchain and provenance gate for Project 0 32X probe work
```

- [ ] **Step 3: Commit the reference note**

Run:

```powershell
git add docs/reference/32x_project0_reference_pass.md docs/reference/README.md
git commit -m "docs: record 32x project0 reference pass"
```

## Task 5: Verify The Scaffold

**Files:**
- Read: `tools/check_32x_project0_env.ps1`
- Read: `probes/32x_project0/README.md`
- Read: `probes/32x_project0/project0_probe.h`
- Read: `tests/test_project0_probe_layout.c`
- Read: `docs/reference/32x_project0_reference_pass.md`

- [ ] **Step 1: Run host tests**

Run:

```powershell
C:\SDKS\SGDK\bin\make.exe -r -f Makefile host-tests
```

Expected:

```text
test_dirty_rect.exe
test_basic_program.exe
test_framebuffer.exe
test_storage_policy.exe
test_project0_probe_layout.exe
```

The command exits `0`.

- [ ] **Step 2: Run the environment checker**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_32x_project0_env.ps1
```

Expected on a machine without SH-2 tooling:

```text
project0_env_ok=False
```

Expected on a machine with SH-2 tooling configured:

```text
project0_env_ok=True
```

Both outcomes are acceptable as long as the printed tool paths and missing-tool
diagnostics are correct.

- [ ] **Step 3: Check for whitespace errors**

Run:

```powershell
git diff --check
```

Expected:

```text
```

No output and exit `0`.

- [ ] **Step 4: Confirm only intended files changed**

Run:

```powershell
git status --short
```

Expected staged or committed files from this plan only, plus any pre-existing
untracked `a.out` left untouched.

## Task 6: Update The Research Decision State

**Files:**
- Modify: `docs/reference/full_stack_cleanroom_research.md`

- [ ] **Step 1: Add a Phase 0 status note**

In `docs/reference/full_stack_cleanroom_research.md`, under
`## Recommended Next Step`, add this paragraph before `Order:`:

```markdown
Phase 0 implementation starts with a non-invasive scaffold: environment
detection for 32X toolchains, a Project 0 runbook, a host-tested result schema,
and a provenance note for 32X references. This phase deliberately does not add
32X boot code until the SH-2 toolchain and source-reference pass are explicit.
```

- [ ] **Step 2: Commit the status note**

Run:

```powershell
git add docs/reference/full_stack_cleanroom_research.md
git commit -m "docs: mark project0 phase0 scaffold"
```

## Self-Review

Spec coverage:

- Covers the four-target research plan by creating a Project 0 scaffold.
- Covers clean-room/provenance requirements before 32X boot code.
- Covers the local toolchain gap by adding an environment checker.
- Covers testability by adding a host-tested `P0_Result` schema.

Placeholder scan:

- The plan contains no red-flag placeholder markers.
- The plan defers 32X boot code by explicit scope, not by a missing step.

Type consistency:

- `P0_Result`, `P0_PROBE_*`, and `P0_STATUS_*` are defined in Task 2 and used
  by the host test in the same task.
- The Makefile include path `-Iprobes/32x_project0` matches the header path.

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-07-01-project0-32x-probe-scaffold.md`. Two execution options:

1. Subagent-Driven (recommended) - dispatch a fresh subagent per task, review between tasks, fast iteration.
2. Inline Execution - execute tasks in this session using executing-plans, batch execution with checkpoints.

Which approach?
