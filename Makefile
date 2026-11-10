ARCH      := x86_64
EFI_INC   := /usr/include/efi
BUILD_DIR := build
TARGET    := $(BUILD_DIR)/aarambh.efi
STB_DIR   := third_party/stb
STB_URL   := https://raw.githubusercontent.com/nothings/stb/master
VERSION   := $(shell sed -n 's/^## \[\([0-9][^]]*\)\].*/\1/p' CHANGELOG.md | head -n1)

NO_BUILD_GOALS := themes debug test uninstall clean

find_gnu_efi = $(shell find /usr/lib /usr/lib64 /usr/local/lib -name '$(1)' 2>/dev/null | head -n1)

CRT0    := $(call find_gnu_efi,crt0-efi-$(ARCH).o)
LDS     := $(call find_gnu_efi,elf_$(ARCH)_efi.lds)
LIBDIR  := $(dir $(call find_gnu_efi,libgnuefi.a))

ifneq ($(MAKECMDGOALS),$(filter $(NO_BUILD_GOALS),$(MAKECMDGOALS)))
BUILD_GOALS := yes
endif
ifeq ($(MAKECMDGOALS),)
BUILD_GOALS := yes
endif

ifeq ($(BUILD_GOALS),yes)
ifneq ($(shell uname -m),$(ARCH))
$(error Aarambh supports 64-bit x86 PCs (x86_64) only, this machine is $(shell uname -m))
endif

ifeq ($(CRT0),)
$(error crt0-efi-$(ARCH).o not found. Install gnu-efi with the command for your distribution in the README (Quick start))
endif

ifeq ($(LDS),)
$(error elf_$(ARCH)_efi.lds not found. Install gnu-efi with the command for your distribution in the README (Quick start))
endif
endif

INCLUDES := -I$(EFI_INC) \
            -I$(EFI_INC)/$(ARCH) \
            -I$(EFI_INC)/protocol \
            -Iinclude \
            -Isrc \
            -I$(STB_DIR)

CFLAGS := $(INCLUDES) \
          -O2 -std=gnu11 \
          -fpic -fshort-wchar -ffreestanding \
          -fno-stack-protector -fno-strict-aliasing -fno-stack-check \
          -mno-red-zone -maccumulate-outgoing-args \
          -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
          -DGNU_EFI_USE_MS_ABI -DEFI_FUNCTION_WRAPPER \
          -DAARAMBH_VERSION='"$(VERSION)"' \
          -MMD -MP \
          -Wall -Wextra -Wno-unused-parameter

LDFLAGS := -nostdlib -znocombreloc -shared -Bsymbolic -T $(LDS) -L$(LIBDIR)

EFI_SECTIONS := -j .text -j .sdata -j .data -j .rodata -j .dynamic -j .dynsym \
                -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc -j .sbat

STB_HEADERS := $(STB_DIR)/stb_image.h $(STB_DIR)/stb_truetype.h

SOURCES := $(shell find src -name '*.c') $(STB_DIR)/stb_impl.c
OBJECTS := $(SOURCES:%.c=$(BUILD_DIR)/obj/%.o)
DEPENDS := $(OBJECTS:.o=.d)

.PHONY: all clean assets install uninstall test qemu themes debug

all: $(TARGET)

$(STB_DIR)/%.h:
	curl -sSfL -o $@ $(STB_URL)/$*.h

$(BUILD_DIR)/obj/%.o: %.c | $(STB_HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/obj/src/sbat.o: CHANGELOG.md

$(BUILD_DIR)/aarambh.so: $(OBJECTS)
	$(LD) $(LDFLAGS) $(CRT0) $(OBJECTS) -o $@ -lefi -lgnuefi
	@if [ -n "$$(nm -u $@)" ]; then \
		echo "Undefined symbols:"; nm -u $@; rm -f $@; exit 1; \
	fi

$(TARGET): $(BUILD_DIR)/aarambh.so
	objcopy $(EFI_SECTIONS) -O efi-app-$(ARCH) --subsystem=10 $< $@

assets:
	./scripts/make-assets.sh

install: $(TARGET)
	./scripts/install.sh $(if $(THEME),--theme $(THEME)) $(if $(DEFAULT),--default $(DEFAULT))

themes:
	@./scripts/install.sh --list-themes

debug:
	@bash hooks/bin/aarambh-debug

uninstall:
	sudo bash hooks/uninstall.sh

test:
	./tests/hooks/run.sh

qemu: $(TARGET)
	./tests/qemu/run.sh $(if $(THEME),--theme $(THEME)) $(if $(DEFAULT),--default $(DEFAULT)) \
		$(if $(SECURE_BOOT),--secure-boot) $(if $(SECOND_ESP),--second-esp)

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPENDS)