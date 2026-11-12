# Copy a kernel and its initramfs onto the ESP for direct boot. Sourced.
#
# With Secure Boot signing set up (signing.sh), the copy is signed with this
# OS's key, so shim accepts it whichever distribution's shim starts Aarambh.
#
# Every copy is a generation: vmlinuz-<id> and initrd-<id>.img, where <id>
# comes from the content of both files and the signing key. The kernel list
# names the pair it boots, so a kernel is never started with the initramfs of
# another one, even when an update is interrupted halfway.

ESP_SPACE_MARGIN=$((8 * 1024 * 1024))

# Limits of aarambh.efi (include/aarambh/entry.h, src/boot/linux.c). A longer
# value would be cut short at boot, so the kernel is listed without direct
# boot instead.
KERNEL_PATH_MAX=159
INITRD_LIST_MAX=383
INITRD_PATH_MAX=159
INITRD_PARTS_MAX=4
CMDLINE_MAX=2047

COPIED_KERNEL=""
COPIED_INITRD=""

esp_free_bytes() {
    df --output=avail -B1 "$ESP_DIR" | tail -n1
}

has_room_for() {
    local needed=$1
    local available

    available=$(esp_free_bytes)
    [ -n "$available" ] && [ "$needed" -le $((available - ESP_SPACE_MARGIN)) ]
}

disable_direct_boot() {
    warn "$1, direct boot disabled (falling back to the OS boot loader)"
}

# Concatenate the initramfs parts (microcode first) into one file.
merged_initrd() {
    local output=$1
    shift

    if [ "$#" -eq 1 ]; then
        echo "$1"
        return 0
    fi

    cat "$@" > "$output" || return 1
    echo "$output"
}

# Names the pair: it changes when the kernel, the initramfs or the signing
# key changes.
generation_id() {
    local id

    id=$({ sha256sum < "$1" && sha256sum < "$2" && signing_id; } | sha256sum | cut -c1-12) || return 1
    [ "${#id}" -eq 12 ] && echo "$id"
}

copy_kernel() {
    if signing_enabled; then
        sign_file "$1" "$2"
    else
        cp "$1" "$2"
    fi
}

# A kernel already on the ESP is started where it is, unless signing is set
# up and it carries no signature at all (shim would refuse it).
boots_in_place() {
    ! signing_enabled || has_signature "$1"
}

remove_other_generations() {
    local keep=$1
    local file

    for file in "$LINUX_DIR"/*; do
        case "$(basename "$file")" in
            "vmlinuz-$keep"|"initrd-$keep.img") ;;
            *) rm -f "$file" ;;
        esac
    done
}

# Both files reach their final names only after a complete copy.
write_generation() {
    local kernel=$1
    local initrd=$2
    local kernel_copy=$3
    local initrd_copy=$4

    if copy_kernel "$kernel" "$kernel_copy.new" && cp "$initrd" "$initrd_copy.new" \
        && mv -f "$initrd_copy.new" "$initrd_copy" && mv -f "$kernel_copy.new" "$kernel_copy"; then
        return 0
    fi

    rm -f "$kernel_copy.new" "$initrd_copy.new" "$kernel_copy" "$initrd_copy"
    return 1
}

publish_generation() {
    local kernel=$1
    local initrd=$2
    local id
    local kernel_copy
    local initrd_copy
    local needed

    if ! id=$(generation_id "$kernel" "$initrd"); then
        disable_direct_boot "reading the kernel or its initramfs failed"
        return 1
    fi

    kernel_copy="$LINUX_DIR/vmlinuz-$id"
    initrd_copy="$LINUX_DIR/initrd-$id.img"

    if [ ! -s "$kernel_copy" ] || ! cmp -s "$initrd" "$initrd_copy"; then
        needed=$(( $(file_size "$kernel") + $(file_size "$initrd") ))

        if ! has_room_for "$needed"; then
            remove_other_generations "$id"
        fi

        if ! has_room_for "$needed"; then
            disable_direct_boot "not enough space on the EFI partition"
            return 1
        fi

        if ! write_generation "$kernel" "$initrd" "$kernel_copy" "$initrd_copy"; then
            disable_direct_boot "copying or signing the kernel failed"
            return 1
        fi

        sync
    fi

    COPIED_KERNEL="vmlinuz-$id"
    COPIED_INITRD="initrd-$id.img"
}

# copy_for_direct_boot <kernel> <initrd> [<initrd>...]
#
# On success COPIED_KERNEL and COPIED_INITRD name the pair on the ESP. On any
# failure it returns 1 and the caller lists the kernel without direct boot.
copy_for_direct_boot() {
    local kernel=$1
    local scratch
    local initrd
    local part
    local status=0

    shift
    COPIED_KERNEL=""
    COPIED_INITRD=""

    for part in "$kernel" "$@"; do
        if [ ! -f "$part" ]; then
            return 1
        fi
    done

    if ! mkdir -p "$LINUX_DIR" || ! scratch=$(mktemp -p "${TMPDIR:-/var/tmp}" aarambh-initrd.XXXXXX); then
        disable_direct_boot "no place to prepare the kernel copy"
        return 1
    fi

    if ! initrd=$(merged_initrd "$scratch" "$@"); then
        disable_direct_boot "joining the initramfs parts failed"
        status=1
    elif ! publish_generation "$kernel" "$initrd"; then
        status=1
    fi

    rm -f "$scratch"
    return "$status"
}

# Remove every kernel copy the published kernel list does not boot: older
# generations and whatever an interrupted update left behind.
prune_direct_boot() {
    local file

    if [ ! -d "$LINUX_DIR" ]; then
        return 0
    fi

    for file in "$LINUX_DIR"/*; do
        if [ -e "$file" ] && ! grep -qsF -- "\\$(basename "$file")|" "$KERNEL_LIST"; then
            rm -f "$file"
        fi
    done

    rmdir "$LINUX_DIR" 2>/dev/null || true
}

fits_direct_boot() {
    local kernel=$1
    local initrds=$2
    local cmdline=$3
    local parts
    local part

    if [ "${#kernel}" -gt "$KERNEL_PATH_MAX" ] || [ "${#initrds}" -gt "$INITRD_LIST_MAX" ] \
        || [ "${#cmdline}" -gt "$CMDLINE_MAX" ]; then
        return 1
    fi

    IFS=',' read -r -a parts <<< "$initrds"

    if [ "${#parts[@]}" -gt "$INITRD_PARTS_MAX" ]; then
        return 1
    fi

    for part in ${parts[@]+"${parts[@]}"}; do
        if [ "${#part}" -gt "$INITRD_PATH_MAX" ]; then
            return 1
        fi
    done

    return 0
}

# Kernel-list fields for a direct boot: |kernel|initrd,initrd|cmdline
# Prints nothing when a value is too long for aarambh.efi.
direct_fields() {
    local kernel=$1
    local initrds=$2
    local cmdline=$3

    if ! fits_direct_boot "$kernel" "$initrds" "$cmdline"; then
        disable_direct_boot "${kernel##*\\} has a path or a command line too long for Aarambh (command line: ${#cmdline} of $CMDLINE_MAX characters)"
        return 0
    fi

    printf '|%s|%s|%s' "$kernel" "$initrds" "$cmdline"
}

copied_fields() {
    direct_fields "$EFI_LINUX_DIR\\$COPIED_KERNEL" "$EFI_LINUX_DIR\\$COPIED_INITRD" "$1"
}

# ESP paths for files already on the ESP, comma separated; fails otherwise.
esp_paths() {
    local joined=""
    local path
    local file

    for file in "$@"; do
        path=$(esp_path "$file") || return 1
        if [ -n "$joined" ]; then
            joined="$joined,$path"
        else
            joined=$path
        fi
    done

    printf '%s' "$joined"
}
