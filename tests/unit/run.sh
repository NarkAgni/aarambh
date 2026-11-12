#!/bin/bash
#
# Build parts of aarambh.efi as normal Linux programs and run them against
# mocked UEFI services, to test what cannot be reached in QEMU: damaged files,
# failed allocations, failed writes. With AddressSanitizer available, a read
# or write outside a buffer fails the test.
#
# Needs gcc and the gnu-efi headers (the same packages as the build).
#
# Usage: tests/unit/run.sh [test...]

set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="$REPO/build/unit"
EFI_INC="${EFI_INC:-/usr/include/efi}"
FAILURES=0

CFLAGS="-I$EFI_INC -I$EFI_INC/x86_64 -I$EFI_INC/protocol -Iinclude -Isrc -Ithird_party/stb -Itests/unit
        -std=gnu11 -g -O1 -fshort-wchar -fno-strict-aliasing -DGNU_EFI_USE_MS_ABI
        -Wall -Wextra -Wno-unused-parameter -Werror=implicit-function-declaration"
# stb_truetype reads 32-bit values as (byte << 24) on an int, which UBSan
# calls a shift overflow; it is how stb is written, not a bounds problem.
SANITIZE="-fsanitize=address,undefined -fno-sanitize=alignment,shift-base -fno-sanitize-recover=all"

SUPPORT="tests/unit/support/efi_mock.c"
LIBRARY="src/lib/memory.c src/lib/str.c src/lib/math.c"
FONTS="src/gfx/font.c src/gfx/font_check.c src/gfx/font_cmap.c src/gfx/font_glyf.c third_party/stb/stb_impl.c"
TEXT="src/gfx/text.c src/gfx/image.c"
IMAGES="src/gfx/image.c src/gfx/image_effects.c src/gfx/image_scale.c src/gfx/sample.c src/gfx/color.c"

# The boot menu never frees its glyphs and images; only bad accesses matter here.
export ASAN_OPTIONS="detect_leaks=0"

cd "$REPO" || exit 1
mkdir -p "$BUILD"

if [ ! -f "$EFI_INC/efi.h" ]; then
    echo "tests/unit: $EFI_INC/efi.h not found, install gnu-efi (see the README)"
    exit 1
fi

# Some distributions ship gcc without the sanitizer libraries.
if ! echo 'int main(void) { return 0; }' \
    | ${CC:-cc} $SANITIZE -x c - -o "$BUILD/probe" 2>/dev/null; then
    echo "note: AddressSanitizer is not installed (libasan, libubsan); running without it"
    SANITIZE=""
fi

# unit <name> <source...>: build tests/unit/<name>_test.c with the sources and run it.
unit() {
    local name=$1

    shift
    echo "== $name"

    # shellcheck disable=SC2086
    if ! ${CC:-cc} $CFLAGS $SANITIZE "tests/unit/${name}_test.c" $SUPPORT "$@" \
        -o "$BUILD/$name" 2> "$BUILD/$name.log"; then
        echo "  FAIL: does not build"
        sed 's/^/    /' "$BUILD/$name.log"
        FAILURES=$((FAILURES + 1))
        return
    fi

    if ! "$BUILD/$name"; then
        echo "  FAIL: $name"
        FAILURES=$((FAILURES + 1))
    fi
}

# shellcheck disable=SC2086
run() {
    case "$1" in
        font) unit font $LIBRARY $FONTS ;;
        font_structure) unit font_structure $LIBRARY $FONTS $TEXT tests/unit/support/font_edit.c ;;
        text) unit text $LIBRARY $FONTS $TEXT tests/unit/support/font_edit.c ;;
        assets) unit assets $LIBRARY $FONTS $IMAGES src/core/assets.c ;;
        kernels) unit kernels $LIBRARY src/core/kernels.c src/boot/linux.c ;;
        navigation) unit navigation $LIBRARY src/core/navigation.c src/core/tween.c src/core/entry.c src/ui/easing.c ;;
        launcher) unit launcher $LIBRARY src/boot/launcher.c src/boot/next_boot.c src/core/entry.c ;;
        fs) unit fs $LIBRARY src/efi/fs.c ;;
        config) unit config $LIBRARY src/core/config.c src/core/navigation.c src/core/tween.c src/core/entry.c src/ui/easing.c ;;
        *) echo "unknown test: $1"; FAILURES=$((FAILURES + 1)) ;;
    esac
}

for name in ${*:-font font_structure text assets kernels navigation launcher fs config}; do
    run "$name"
done

echo
if [ "$FAILURES" = 0 ]; then
    echo "all unit tests passed"
else
    echo "$FAILURES unit test(s) failed"
    exit 1
fi
