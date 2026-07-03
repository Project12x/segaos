# Master Makefile for Sega CD OS (SegaOS)
#
# Uses SGDK's m68k-elf-gcc toolchain for both CPUs.
# IP (Main CPU) and SP (Sub CPU) are built separately,
# then packed into a boot sector ISO by build_iso.py.

# ============================================================
# Toolchain (SGDK ships non-prefixed m68k-elf binaries)
# ============================================================
SGDK_BIN  ?= C:/SDKS/SGDK/bin
SGDK_LIB  ?= C:/SDKS/SGDK/lib
HOST_CC   ?= C:/Qt/Tools/mingw1310_64/bin/gcc.exe
CD_REGION ?= US
BOOT_PROBE ?= 0
BOOT_PROBE_FRAMEBUFFER ?= 0
BOOT_SAFE_DESKTOP ?= 1
SUB_RUNTIME_SMOKE ?= 0
DESKTOP_INIT_PROBE ?= 0
DESKTOP_REPEAT_PROBE ?= 0
DESKTOP_LOOP_PROBE ?= 0
DESKTOP_TIMING_PROBE ?= 0
DESKTOP_WM_PROBE ?= 0
DESKTOP_DIRTY_QUEUE_PROBE ?= 0
DESKTOP_SCHEDULER_PROBE ?= 0
DESKTOP_PUMP_PROBE ?= 0
BASIC_BRAM_PROBE ?= 0
BOOT_SAFE_LIVE_PROBE ?= 0
BOOT_SAFE_TEXT_PROBE ?= 0
BOOT_SAFE_TITLE_PROBE ?= 0
BOOT_SAFE_VISUAL_PROBE ?= 0
VDP_TEXT_PROBE ?= 0

CC        = $(SGDK_BIN)/gcc.exe
AS        = $(SGDK_BIN)/as.exe
LD        = $(SGDK_BIN)/ld.exe
OBJCOPY   = $(SGDK_BIN)/objcopy.exe
SIZE      = $(SGDK_BIN)/size.exe
MAKE_CMD  = $(SGDK_BIN)/make.exe
MKDIR     = $(SGDK_BIN)/mkdir.exe
PYTHON_CANDIDATES := $(wildcard C:/Users/estee/AppData/Local/Programs/Python/Python312/python.exe) \
                     $(wildcard C:/Users/estee/AppData/Local/Programs/Python/Python314/python.exe)
PYTHON    ?= $(firstword $(PYTHON_CANDIDATES) python)

# ============================================================
# Flags
# ============================================================
CFLAGS_COMMON = -m68000 -Wall -Wextra -O2 -ffreestanding -fno-builtin \
                -fomit-frame-pointer \
                -ffunction-sections -fdata-sections \
                -nostdinc -isystem include/libc -Iinclude \
                -B $(SGDK_BIN)/

CFLAGS_SUB  = $(CFLAGS_COMMON) -DSUB_CPU
CFLAGS_MAIN = $(CFLAGS_COMMON) -DMAIN_CPU -DJP=1 -DUS=2 -DEU=3 -DREGION=$(CD_REGION)
ifeq ($(BOOT_PROBE),1)
CFLAGS_SUB  += -DBOOT_PROBE
CFLAGS_MAIN += -DBOOT_PROBE
endif
ifeq ($(BOOT_SAFE_DESKTOP),1)
CFLAGS_SUB  += -DBOOT_SAFE_DESKTOP
CFLAGS_MAIN += -DBOOT_SAFE_DESKTOP
CFLAGS_MAIN += -Os
endif
ifeq ($(SUB_RUNTIME_SMOKE),1)
CFLAGS_SUB  += -DSUB_RUNTIME_SMOKE
CFLAGS_MAIN += -DSUB_RUNTIME_SMOKE
endif
ifeq ($(DESKTOP_INIT_PROBE),1)
CFLAGS_SUB  += -DDESKTOP_INIT_PROBE
CFLAGS_MAIN += -DDESKTOP_INIT_PROBE
endif
ifeq ($(DESKTOP_REPEAT_PROBE),1)
CFLAGS_SUB  += -DDESKTOP_INIT_PROBE -DDESKTOP_REPEAT_PROBE
CFLAGS_MAIN += -DDESKTOP_INIT_PROBE -DDESKTOP_REPEAT_PROBE
endif
ifeq ($(DESKTOP_LOOP_PROBE),1)
CFLAGS_SUB  += -DDESKTOP_INIT_PROBE -DDESKTOP_LOOP_PROBE
CFLAGS_MAIN += -DDESKTOP_INIT_PROBE -DDESKTOP_LOOP_PROBE
endif
ifeq ($(DESKTOP_TIMING_PROBE),1)
CFLAGS_SUB  += -DDESKTOP_INIT_PROBE -DDESKTOP_TIMING_PROBE
CFLAGS_MAIN += -DDESKTOP_INIT_PROBE -DDESKTOP_TIMING_PROBE
endif
ifeq ($(DESKTOP_WM_PROBE),1)
CFLAGS_SUB  += -DDESKTOP_INIT_PROBE -DDESKTOP_WM_PROBE
CFLAGS_MAIN += -DDESKTOP_INIT_PROBE -DDESKTOP_WM_PROBE
endif
ifeq ($(DESKTOP_DIRTY_QUEUE_PROBE),1)
CFLAGS_SUB  += -DDESKTOP_INIT_PROBE -DDESKTOP_DIRTY_QUEUE_PROBE
CFLAGS_MAIN += -DDESKTOP_INIT_PROBE -DDESKTOP_DIRTY_QUEUE_PROBE -Os
endif
ifeq ($(DESKTOP_SCHEDULER_PROBE),1)
CFLAGS_SUB  += -DDESKTOP_INIT_PROBE -DDESKTOP_SCHEDULER_PROBE
CFLAGS_MAIN += -DDESKTOP_INIT_PROBE -DDESKTOP_SCHEDULER_PROBE -Os
endif
ifeq ($(DESKTOP_PUMP_PROBE),1)
CFLAGS_SUB  += -DDESKTOP_INIT_PROBE -DDESKTOP_PUMP_PROBE
CFLAGS_MAIN += -DDESKTOP_INIT_PROBE -DDESKTOP_PUMP_PROBE -Os
endif
ifeq ($(BASIC_BRAM_PROBE),1)
CFLAGS_SUB  += -DBASIC_BRAM_PROBE
CFLAGS_MAIN += -DBASIC_BRAM_PROBE
endif
ifeq ($(BOOT_SAFE_LIVE_PROBE),1)
CFLAGS_SUB  += -DBOOT_SAFE_LIVE_PROBE
CFLAGS_MAIN += -DBOOT_SAFE_LIVE_PROBE -Os
endif
ifeq ($(BOOT_SAFE_TEXT_PROBE),1)
CFLAGS_SUB  += -DBOOT_SAFE_TEXT_PROBE
CFLAGS_MAIN += -DBOOT_SAFE_TEXT_PROBE
endif
ifeq ($(BOOT_SAFE_TITLE_PROBE),1)
CFLAGS_SUB  += -DBOOT_SAFE_TITLE_PROBE
CFLAGS_MAIN += -DBOOT_SAFE_TITLE_PROBE
endif
ifeq ($(BOOT_SAFE_VISUAL_PROBE),1)
CFLAGS_MAIN += -DBOOT_SAFE_VISUAL_PROBE
endif
ifeq ($(VDP_TEXT_PROBE),1)
CFLAGS_MAIN += -DVDP_TEXT_PROBE
endif
ifeq ($(BOOT_PROBE_FRAMEBUFFER),1)
CFLAGS_MAIN += -DBOOT_PROBE_FRAMEBUFFER
endif
ASFLAGS     = -m68000 --register-prefix-optional
ifeq ($(BOOT_PROBE),1)
ASFLAGS     += --defsym BOOT_PROBE=1
endif

# ============================================================
# Directories
# ============================================================
SUB_DIR   = src/sub
MAIN_DIR  = src/main
BUILD_DIR = build

# ============================================================
# Output Files
# ============================================================
SUB_ELF   = $(BUILD_DIR)/sub_cpu.elf
SUB_BIN   = $(BUILD_DIR)/sub_cpu.bin
SUB_MAP   = $(BUILD_DIR)/sub_cpu.map
MAIN_ELF  = $(BUILD_DIR)/main_cpu.elf
MAIN_BIN  = $(BUILD_DIR)/main_cpu.bin
MAIN_MAP  = $(BUILD_DIR)/main_cpu.map
DISC_ISO  = $(BUILD_DIR)/segaos.iso
DISC_CUE  = $(BUILD_DIR)/segaos.cue

# ============================================================
# Sub CPU Sources
# ============================================================
SUB_ASM_SRCS = $(SUB_DIR)/crt0.s
ifeq ($(BOOT_PROBE),1)
SUB_C_SRCS   = $(SUB_DIR)/sub.c
else
ifeq ($(SUB_RUNTIME_SMOKE),1)
SUB_C_SRCS   = $(SUB_DIR)/runtime_smoke.c
else
ifeq ($(BOOT_SAFE_DESKTOP),1)
SUB_ASM_SRCS += $(SUB_DIR)/bram_bios_68k.s
SUB_C_SRCS   = $(SUB_DIR)/blitter.c \
               $(SUB_DIR)/basic.c \
               $(SUB_DIR)/basic_bram_storage.c \
               $(SUB_DIR)/basic_bram_smoke.c \
               $(SUB_DIR)/basic_storage.c \
               $(SUB_DIR)/bram.c \
               $(SUB_DIR)/bram_bios.c \
               $(SUB_DIR)/dirty_rect.c \
               $(SUB_DIR)/external_cart.c \
               $(SUB_DIR)/libc.c \
               $(SUB_DIR)/mem.c \
               $(SUB_DIR)/storage.c \
               $(SUB_DIR)/sub.c \
               $(SUB_DIR)/sysfont.c \
               $(SUB_DIR)/wm.c
else
SUB_ASM_SRCS += $(SUB_DIR)/bram_bios_68k.s
SUB_C_SRCS   = $(filter-out $(SUB_DIR)/runtime_smoke.c, \
               $(wildcard $(SUB_DIR)/*.c))
endif
endif
endif

SUB_ASM_OBJS = $(SUB_ASM_SRCS:$(SUB_DIR)/%.s=$(BUILD_DIR)/sub_%.o)
SUB_C_OBJS   = $(SUB_C_SRCS:$(SUB_DIR)/%.c=$(BUILD_DIR)/sub_%.o)
SUB_OBJS     = $(SUB_ASM_OBJS) $(SUB_C_OBJS)

# ============================================================
# Main CPU Sources
# ============================================================
MAIN_ASM_SRCS = $(MAIN_DIR)/crt0.s
MAIN_C_SRCS   = $(wildcard $(MAIN_DIR)/*.c)

MAIN_ASM_OBJS = $(MAIN_ASM_SRCS:$(MAIN_DIR)/%.s=$(BUILD_DIR)/main_%.o)
MAIN_C_OBJS   = $(MAIN_C_SRCS:$(MAIN_DIR)/%.c=$(BUILD_DIR)/main_%.o)
MAIN_OBJS     = $(MAIN_ASM_OBJS) $(MAIN_C_OBJS)

# ============================================================
# Targets
# ============================================================
.PHONY: all clean sub main iso dirs info host-tests

all: iso

# Build Sub CPU
sub: dirs $(SUB_BIN)
	@echo "Sub CPU build complete: $(SUB_BIN)"
	@$(SIZE) $(SUB_ELF) || true

# Build Main CPU (IP - no Sub CPU embedding, BIOS loads SP separately)
main: dirs $(MAIN_BIN)
	@echo "Main CPU build complete: $(MAIN_BIN)"
	@$(SIZE) $(MAIN_ELF) || true

# Build boot disc ISO/CUE (requires both sub and main)
iso: sub main
	@echo "Building ISO disc image..."
	$(PYTHON) tools/build_iso.py $(MAIN_BIN) $(SUB_BIN) $(BUILD_DIR)/segaos $(CD_REGION)
	$(PYTHON) tools/verify_disc.py $(DISC_ISO) --cue $(DISC_CUE) --ip $(MAIN_BIN) --sp $(SUB_BIN) --region $(CD_REGION)
	@echo "Disc image complete: $(DISC_ISO)"

dirs:
	@$(MKDIR) -p $(BUILD_DIR) 2>/dev/null || mkdir $(BUILD_DIR) 2>NUL || true

info:
	@echo "SGDK_BIN: $(SGDK_BIN)"
	@echo "CC:       $(CC)"
	@echo "HOST_CC:  $(HOST_CC)"
	@echo "PYTHON:   $(PYTHON)"
	@echo "CD_REGION: $(CD_REGION)"
	@echo "BOOT_PROBE: $(BOOT_PROBE)"
	@echo "BOOT_PROBE_FRAMEBUFFER: $(BOOT_PROBE_FRAMEBUFFER)"
	@echo "BOOT_SAFE_DESKTOP: $(BOOT_SAFE_DESKTOP)"
	@echo "SUB_RUNTIME_SMOKE: $(SUB_RUNTIME_SMOKE)"
	@echo "DESKTOP_INIT_PROBE: $(DESKTOP_INIT_PROBE)"
	@echo "DESKTOP_REPEAT_PROBE: $(DESKTOP_REPEAT_PROBE)"
	@echo "DESKTOP_LOOP_PROBE: $(DESKTOP_LOOP_PROBE)"
	@echo "DESKTOP_TIMING_PROBE: $(DESKTOP_TIMING_PROBE)"
	@echo "DESKTOP_WM_PROBE: $(DESKTOP_WM_PROBE)"
	@echo "DESKTOP_DIRTY_QUEUE_PROBE: $(DESKTOP_DIRTY_QUEUE_PROBE)"
	@echo "DESKTOP_SCHEDULER_PROBE: $(DESKTOP_SCHEDULER_PROBE)"
	@echo "DESKTOP_PUMP_PROBE: $(DESKTOP_PUMP_PROBE)"
	@echo "BASIC_BRAM_PROBE: $(BASIC_BRAM_PROBE)"
	@echo "BOOT_SAFE_LIVE_PROBE: $(BOOT_SAFE_LIVE_PROBE)"
	@echo "BOOT_SAFE_TEXT_PROBE: $(BOOT_SAFE_TEXT_PROBE)"
	@echo "BOOT_SAFE_TITLE_PROBE: $(BOOT_SAFE_TITLE_PROBE)"
	@echo "BOOT_SAFE_VISUAL_PROBE: $(BOOT_SAFE_VISUAL_PROBE)"
	@echo "VDP_TEXT_PROBE: $(VDP_TEXT_PROBE)"
	@echo "Sub sources: $(SUB_C_SRCS)"
	@echo "Sub ASM:     $(SUB_ASM_SRCS)"
	@echo "Sub objects:  $(SUB_OBJS)"
	@echo "Main sources: $(MAIN_C_SRCS)"
	@echo "Main objects: $(MAIN_OBJS)"

host-tests: dirs
	$(HOST_CC) -std=c99 -Wall -Wextra -Iinclude tests/test_dirty_rect.c src/sub/dirty_rect.c -o $(BUILD_DIR)/test_dirty_rect.exe
	$(BUILD_DIR)/test_dirty_rect.exe
	$(HOST_CC) -std=c99 -Wall -Wextra -Iinclude tests/test_bram.c src/sub/bram.c src/sub/storage.c -o $(BUILD_DIR)/test_bram.exe
	$(BUILD_DIR)/test_bram.exe
	$(HOST_CC) -std=c99 -Wall -Wextra -Iinclude tests/test_bram_bios.c src/sub/bram.c src/sub/bram_bios.c src/sub/storage.c -o $(BUILD_DIR)/test_bram_bios.exe
	$(BUILD_DIR)/test_bram_bios.exe
	$(HOST_CC) -std=c99 -Wall -Wextra -Iinclude tests/test_basic_program.c src/sub/basic.c -o $(BUILD_DIR)/test_basic_program.exe
	$(BUILD_DIR)/test_basic_program.exe
	$(HOST_CC) -std=c99 -Wall -Wextra -Iinclude tests/test_basic_storage.c src/sub/basic.c src/sub/basic_storage.c src/sub/storage.c -o $(BUILD_DIR)/test_basic_storage.exe
	$(BUILD_DIR)/test_basic_storage.exe
	$(HOST_CC) -std=c99 -Wall -Wextra -Iinclude tests/test_basic_bram_storage.c src/sub/basic.c src/sub/basic_storage.c src/sub/basic_bram_storage.c src/sub/bram.c src/sub/storage.c -o $(BUILD_DIR)/test_basic_bram_storage.exe
	$(BUILD_DIR)/test_basic_bram_storage.exe
	$(HOST_CC) -std=c99 -Wall -Wextra -Iinclude tests/test_basic_bram_smoke.c src/sub/basic.c src/sub/basic_storage.c src/sub/basic_bram_storage.c src/sub/basic_bram_smoke.c src/sub/bram.c src/sub/storage.c -o $(BUILD_DIR)/test_basic_bram_smoke.exe
	$(BUILD_DIR)/test_basic_bram_smoke.exe
	$(HOST_CC) -std=c99 -Wall -Wextra -DFB_HOST_TEST -Iinclude tests/test_framebuffer.c src/main/framebuffer.c src/sub/dirty_rect.c -o $(BUILD_DIR)/test_framebuffer.exe
	$(BUILD_DIR)/test_framebuffer.exe
	$(HOST_CC) -std=c99 -Wall -Wextra -DFB_HOST_TEST -Iinclude tests/test_frame_scheduler.c src/main/frame_scheduler.c src/sub/dirty_rect.c -o $(BUILD_DIR)/test_frame_scheduler.exe
	$(BUILD_DIR)/test_frame_scheduler.exe
	$(HOST_CC) -std=c99 -Wall -Wextra -DFB_HOST_TEST -Iinclude tests/test_frame_upload_pump.c src/main/frame_upload_pump.c src/main/frame_scheduler.c -o $(BUILD_DIR)/test_frame_upload_pump.exe
	$(BUILD_DIR)/test_frame_upload_pump.exe
	$(HOST_CC) -std=c99 -Wall -Wextra -Iinclude tests/test_boot_frame_marker.c -o $(BUILD_DIR)/test_boot_frame_marker.exe
	$(BUILD_DIR)/test_boot_frame_marker.exe
	$(HOST_CC) -std=c99 -Wall -Wextra -Iinclude tests/test_boot_live_probe.c -o $(BUILD_DIR)/test_boot_live_probe.exe
	$(BUILD_DIR)/test_boot_live_probe.exe
	$(HOST_CC) -std=c99 -Wall -Wextra -Iinclude tests/test_storage_policy.c src/sub/storage.c -o $(BUILD_DIR)/test_storage_policy.exe
	$(BUILD_DIR)/test_storage_policy.exe
	$(HOST_CC) -std=c99 -Wall -Wextra -Iinclude tests/test_external_cart_probe.c src/sub/external_cart.c src/sub/storage.c -o $(BUILD_DIR)/test_external_cart_probe.exe
	$(BUILD_DIR)/test_external_cart_probe.exe
	powershell -NoProfile -ExecutionPolicy Bypass -File tests/test_probe_timeout.ps1 -HostCc "$(HOST_CC)"

# ============================================================
# Sub CPU Build Rules
# ============================================================

# Assemble crt0.s
$(BUILD_DIR)/sub_%.o: $(SUB_DIR)/%.s
	$(AS) $(ASFLAGS) -o $@ $<

# Compile Sub CPU C files
$(BUILD_DIR)/sub_%.o: $(SUB_DIR)/%.c
	$(CC) $(CFLAGS_SUB) -c $< -o $@

# Link Sub CPU ELF
$(SUB_ELF): $(SUB_OBJS) $(SUB_DIR)/sub.ld
	$(LD) -T $(SUB_DIR)/sub.ld -Map=$(SUB_MAP) --gc-sections \
		-o $@ $(SUB_OBJS) -L$(SGDK_LIB) -lgcc

# Convert to raw binary
$(SUB_BIN): $(SUB_ELF)
	$(OBJCOPY) -O binary $< $@
	@echo "Sub CPU binary size:"
	@$(SIZE) $< || true

# ============================================================
# Main CPU Build Rules (IP only, no Sub CPU embedding)
# ============================================================

# Assemble Main CPU startup
$(BUILD_DIR)/main_%.o: $(MAIN_DIR)/%.s
	$(AS) $(ASFLAGS) $< -o $@

# Compile Main CPU C files
$(BUILD_DIR)/main_%.o: $(MAIN_DIR)/%.c
	$(CC) $(CFLAGS_MAIN) -c $< -o $@

# Link Main CPU ELF (no sub_cpu_bin.o — BIOS loads SP from boot sector)
$(MAIN_ELF): $(MAIN_OBJS) $(MAIN_DIR)/main.ld
	$(LD) -T $(MAIN_DIR)/main.ld -Map=$(MAIN_MAP) --gc-sections \
		-o $@ $(MAIN_OBJS) -L$(SGDK_LIB) -lgcc

$(MAIN_BIN): $(MAIN_ELF)
	$(OBJCOPY) -O binary $< $@
	@echo "Main CPU binary size:"
	@$(SIZE) $< || true

# ============================================================
# Clean
# ============================================================
clean:
	rm -rf $(BUILD_DIR) 2>/dev/null || rmdir /s /q $(BUILD_DIR) 2>NUL || true

# ============================================================
# Dependencies (auto-generated)
# ============================================================
-include $(SUB_C_OBJS:.o=.d)
-include $(MAIN_OBJS:.o=.d)
