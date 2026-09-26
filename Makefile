# Copyright (c) 2026 Vita2TV contributors
# SPDX-License-Identifier: MIT

BUILD_DIR ?= build
RELEASE_DIR ?= release
CC ?= cc
VITA_CC ?= arm-vita-eabi-gcc
VITA_NM ?= arm-vita-eabi-nm
VITA_ELF_CREATE ?= vita-elf-create
VITA_MAKE_FSELF ?= vita-make-fself
QEMU_ARM ?= qemu-arm
NATIVE_FLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer
MODULE = vita2tv_usb_fix

.PHONY: all check release test test-native test-arm
all: $(BUILD_DIR)/$(MODULE).skprx

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR)/$(MODULE).elf: src/main.c src/phase_checkpoint.S src/phase_guard.h src/phase_install.h src/firmware_guard.h Makefile | $(BUILD_DIR)
	$(VITA_CC) -std=gnu11 -Wall -Wextra -Werror -O2 -nostartfiles \
	  -mcpu=cortex-a9 -mthumb -mthumb-interwork -Wl,-q,-e,module_start src/main.c src/phase_checkpoint.S -o "$@" \
	  -ltaihenForKernel_stub -ltaihenModuleUtils_stub \
	  -lSceIntrmgrForDriver_stub -lSceThreadmgrForDriver_stub \
	  -lSceIofilemgrForDriver_stub -lSceSysclibForDriver_stub

$(BUILD_DIR)/$(MODULE).velf: $(BUILD_DIR)/$(MODULE).elf src/exports.yml
	$(VITA_ELF_CREATE) -e src/exports.yml "$<" "$@"

$(BUILD_DIR)/$(MODULE).skprx: $(BUILD_DIR)/$(MODULE).velf
	$(VITA_MAKE_FSELF) -c "$<" "$@"

check: $(BUILD_DIR)/$(MODULE).elf
	@test -z "$$($(VITA_NM) -u "$<")"
	@! $(VITA_NM) "$<" | grep -Eq '[[:space:]]ksceUdcd(Req|Clear|Activate|Deactivate|Start|Stop)'

$(BUILD_DIR)/phase_model_test: tests/phase_model.c tests/phase_model_test.c tests/phase_model.h Makefile | $(BUILD_DIR)
	$(CC) $(NATIVE_FLAGS) tests/phase_model.c tests/phase_model_test.c -o "$@"

$(BUILD_DIR)/phase_guard_test: tests/phase_guard_test.c src/phase_guard.h src/firmware_guard.h tests/synthetic_fixture.h Makefile | $(BUILD_DIR)
	$(CC) $(NATIVE_FLAGS) -Isrc tests/phase_guard_test.c -o "$@"

$(BUILD_DIR)/phase_install_test: tests/phase_install_test.c src/phase_install.h Makefile | $(BUILD_DIR)
	$(CC) $(NATIVE_FLAGS) -Isrc tests/phase_install_test.c -o "$@"

test: test-native
test-native: $(BUILD_DIR)/phase_model_test $(BUILD_DIR)/phase_guard_test $(BUILD_DIR)/phase_install_test
	$(BUILD_DIR)/phase_model_test
	$(BUILD_DIR)/phase_guard_test
	$(BUILD_DIR)/phase_install_test

# Linux ARM-user executable: Cortex-A9 instructions, no libc or firmware.
$(BUILD_DIR)/phase_checkpoint_test.elf: src/phase_checkpoint.S tests/phase_checkpoint_test.S Makefile | $(BUILD_DIR)
	$(VITA_CC) -nostdlib -static -mcpu=cortex-a9 -mthumb -mthumb-interwork \
	  -Wl,-e,_start src/phase_checkpoint.S tests/phase_checkpoint_test.S -o "$@"

test-arm: $(BUILD_DIR)/phase_checkpoint_test.elf
	$(QEMU_ARM) -cpu cortex-a9 "$<"
	@echo 'PASS: exact trampoline, 3840 cases, registers, stack and flags'

# Update the tracked, ready-to-install binary only after a checked cross-build.
release: all check
	mkdir -p "$(RELEASE_DIR)"
	cp "$(BUILD_DIR)/$(MODULE).skprx" "$(RELEASE_DIR)/$(MODULE).skprx"
