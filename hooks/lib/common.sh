# Shared helpers for every Aarambh shell script. Sourced, not executed.
#
# AARAMBH_ROOT prefixes every system path (/etc, /boot, /proc). It is empty on
# a real system and points at a fake root in tests/hooks.

ROOT="${AARAMBH_ROOT:-}"
ENV_FILE="$ROOT/etc/aarambh.env"
DRY_RUN="${DRY_RUN:-0}"

log() {
    echo "aarambh: $*"
}

warn() {
    echo "aarambh: warning: $*" >&2
}

die() {
    echo "aarambh: error: $*" >&2
    exit 1
}

# Run a command, or only print it with --dry-run.
run() {
    if [ "$DRY_RUN" = 1 ]; then
        echo "  would run: $*"
        return 0
    fi
    "$@"
}

# Write stdin to a file, or only name the file with --dry-run.
write_file() {
    local target=$1

    if [ "$DRY_RUN" = 1 ]; then
        echo "  would write: $target"
        cat >/dev/null
        return 0
    fi

    mkdir -p "$(dirname "$target")"
    cat > "$target"
}

# Replace a file with stdin through one rename. The old file stays untouched
# when the new content is empty or cannot be written completely.
replace_file() {
    local target=$1
    local scratch

    if ! scratch=$(mktemp "$target.XXXXXX"); then
        cat >/dev/null
        return 1
    fi

    if cat > "$scratch" && [ -s "$scratch" ] && mv -f "$scratch" "$target"; then
        return 0
    fi

    rm -f "$scratch"
    return 1
}

CONFIG_LOCK_WAIT_SECONDS=30

# Runs "$@" while holding the lock every writer of aarambh.conf takes (the
# aarambh command, setup.sh, uninstall.sh and aarambh-kernels), so each
# read, change and replace starts from the result of the one before it.
# Two writers otherwise both read the old file and the later rename drops the
# other's change. Hold it only around one such change, never while calling
# another Aarambh script.
with_config_lock() {
    local lock_dir="$ROOT/run/lock"

    if ! has_command flock || ! mkdir -p "$lock_dir" 2>/dev/null; then
        "$@"
        return
    fi

    (
        if ! flock -w "$CONFIG_LOCK_WAIT_SECONDS" 8; then
            warn "another Aarambh command is changing aarambh.conf, try again"
            exit 1
        fi
        "$@"
    ) 8> "$lock_dir/aarambh-config.lock"
}

has_command() {
    command -v "$1" >/dev/null 2>&1
}

load_environment() {
    if [ ! -f "$ENV_FILE" ]; then
        return 1
    fi

    # shellcheck source=/dev/null
    . "$ENV_FILE"

    ESP_ROOT="${ESP_ROOT:-${ESP_DIR%/EFI/aarambh}}"
    BOOTLOADER="${BOOTLOADER:-grub}"
    BLS="${BLS:-no}"
    GRUB_PROBE="${GRUB_PROBE:-}"
    GRUB_CFG="${GRUB_CFG:-/boot/grub/grub.cfg}"
    SIGN_MODE="${SIGN_MODE:-none}"
    SIGN_KEY="${SIGN_KEY:-}"
    SIGN_CERT="${SIGN_CERT:-}"
    SIGN_DER="${SIGN_DER:-}"
    SHIM_DIR="${SHIM_DIR:-}"
    SHIM_FILE="${SHIM_FILE:-}"
    SHIM_MANAGER="${SHIM_MANAGER:-}"
    SHIM_SECOND="${SHIM_SECOND:-}"
}

# Print an ESP-relative UEFI path (\vmlinuz-linux) for a file that lives on the
# ESP, or fail when it lives anywhere else.
esp_path() {
    local file
    local esp

    file=$(realpath -m "$1")
    esp=$(realpath -m "$ESP_ROOT")

    case "$file" in
        "$esp"/*) ;;
        *) return 1 ;;
    esac

    printf '%s' "${file#"$esp"}" | tr '/' '\\'
}

file_size() {
    if [ -f "$1" ]; then
        stat -c %s "$1"
    else
        echo 0
    fi
}
