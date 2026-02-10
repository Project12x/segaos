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

CC        = $(SGDK_BIN)/gcc.exe
AS        = $(SGDK_BIN)/as.exe
LD        = $(SGDK_BIN)/ld.exe
OBJCOPY   = $(SGDK_BIN)/objcopy.exe
SIZE      = $(SGDK_BIN)/size.exe
MAKE_CMD  = $(SGDK_BIN)/make.exe
MKDIR     = $(SGDK_BIN)/mkdir.exe
PYTHON    = python

# ============================================================
# Flags
# ============================================================
CFLAGS_COMMON = -m68000 -Wall -Wextra -O2 -fomit-frame-pointer \
                -ffunction-sections -fdata-sections \
                -nostdinc -isystem include/libc -Iinclude \
                -B $(SGDK_BIN)/

CFLAGS_SUB  = $(CFLAGS_COMMON) -DSUB_CPU
CFLAGS_MAIN = $(CFLAGS_COMMON) -DMAIN_CPU
ASFLAGS     = -m68000 --register-prefix-optional

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
ISO_OUT   = $(BUILD_DIR)/segaos.iso

# ============================================================
# Sub CPU Sources
# ============================================================
SUB_ASM_SRCS = $(SUB_DIR)/crt0.s
SUB_C_SRCS   = $(wildcard $(SUB_DIR)/*.c)

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
.PHONY: all clean sub main iso dirs info

all: iso

# Build Sub CPU
sub: dirs $(SUB_BIN)
	@echo "Sub CPU build complete: $(SUB_BIN)"
	@$(SIZE) $(SUB_ELF) || true

# Build Main CPU (IP - no Sub CPU embedding, BIOS loads SP separately)
main: dirs $(MAIN_BIN)
	@echo "Main CPU build complete: $(MAIN_BIN)"
	@$(SIZE) $(MAIN_ELF) || true

# Build boot disc ISO (requires both sub and main)
iso: sub main
	@echo "Building ISO..."
	$(PYTHON) tools/build_iso.py $(MAIN_BIN) $(SUB_BIN) $(ISO_OUT)
	@echo "ISO build complete: $(ISO_OUT)"

dirs:
	@$(MKDIR) -p $(BUILD_DIR) 2>/dev/null || mkdir $(BUILD_DIR) 2>NUL || true

info:
	@echo "SGDK_BIN: $(SGDK_BIN)"
	@echo "CC:       $(CC)"
	@echo "Sub sources: $(SUB_C_SRCS)"
	@echo "Sub ASM:     $(SUB_ASM_SRCS)"
	@echo "Sub objects:  $(SUB_OBJS)"
	@echo "Main sources: $(MAIN_C_SRCS)"
	@echo "Main objects: $(MAIN_OBJS)"

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
