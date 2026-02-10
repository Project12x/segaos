# Master Makefile for Sega CD OS (SegaOS)
#
# Uses SGDK's m68k-elf-gcc toolchain for both CPUs.
# Sub CPU program is compiled, then embedded into the Main CPU ROM.

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

# Embedded Sub CPU binary as object file
SUB_BIN_OBJ = $(BUILD_DIR)/sub_cpu_bin.o
MAIN_MAP    = $(BUILD_DIR)/main_cpu.map

# ============================================================
# Targets
# ============================================================
.PHONY: all clean sub main dirs info

all: dirs sub

# Build Sub CPU only (Main CPU needs SGDK integration)
sub: dirs $(SUB_BIN)
	@echo "Sub CPU build complete: $(SUB_BIN)"
	@$(SIZE) $(SUB_ELF) || true

# Build Main CPU (requires SGDK - placeholder for now)
main: dirs sub $(MAIN_BIN)
	@echo "Main CPU build complete: $(MAIN_BIN)"

dirs:
	@$(MKDIR) -p $(BUILD_DIR) 2>/dev/null || mkdir $(BUILD_DIR) 2>NUL || true

info:
	@echo "SGDK_BIN: $(SGDK_BIN)"
	@echo "CC:       $(CC)"
	@echo "Sub sources: $(SUB_C_SRCS)"
	@echo "Sub ASM:     $(SUB_ASM_SRCS)"
	@echo "Sub objects:  $(SUB_OBJS)"

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
# Main CPU Build Rules
# ============================================================

# Convert Sub CPU binary into a linkable object file
$(SUB_BIN_OBJ): $(SUB_BIN)
	$(OBJCOPY) -I binary -O elf32-m68k -B m68k \
		--rename-section .data=.rodata,alloc,load,readonly,data,contents \
		$< $@

# Assemble Main CPU startup
$(BUILD_DIR)/main_%.o: $(MAIN_DIR)/%.s
	$(AS) $(ASFLAGS) $< -o $@

# Compile Main CPU C files
$(BUILD_DIR)/main_%.o: $(MAIN_DIR)/%.c
	$(CC) $(CFLAGS_MAIN) -c $< -o $@

# Link Main CPU ELF
# crt0.o must come first (entry point)
$(MAIN_ELF): $(MAIN_OBJS) $(SUB_BIN_OBJ) $(MAIN_DIR)/main.ld
	$(LD) -T $(MAIN_DIR)/main.ld -Map=$(MAIN_MAP) --gc-sections \
		-o $@ $(MAIN_OBJS) $(SUB_BIN_OBJ) -L$(SGDK_LIB) -lgcc

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
