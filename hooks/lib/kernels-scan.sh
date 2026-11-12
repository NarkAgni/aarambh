# Kernel list from the kernel files in /boot. Used by GRUB distributions
# without BLS (Debian, Ubuntu, Arch, openSUSE, Gentoo, Void) and by systems
# with other boot loaders. Sourced.

# Same list as GRUB_EARLY_INITRD_LINUX_STOCK in GRUB.
MICROCODE_IMAGES="intel-uc.img intel-ucode.img amd-uc.img amd-ucode.img early_ucode.cpio microcode.cpio"

load_grub_defaults() {
    local file

    GRUB_DISABLE_SUBMENU=""
    GRUB_DISABLE_RECOVERY=""

    if [ -f "$ROOT/etc/default/grub" ]; then
        # shellcheck source=/dev/null
        . "$ROOT/etc/default/grub"
    fi

    for file in "$ROOT"/etc/default/grub.d/*.cfg; do
        if [ -f "$file" ]; then
            # shellcheck source=/dev/null
            . "$file"
        fi
    done
}

# Same rule as upstream GRUB's 10_linux: drop everything before the last "-"
# that precedes the first digit. Used for GRUB menu ids.
grub_version() {
    echo "$1" | sed -e 's,^[^0-9]*-,,'
}

# The kernel file name without its prefix: 6.8.0-45-generic, linux, linux-lts.
kernel_name() {
    local name=$1

    name=${name#vmlinuz-}
    name=${name#vmlinux-}
    name=${name#kernel-}
    echo "$name"
}

kernel_title() {
    local version=$1

    case "$version" in
        linux) echo "Linux" ;;
        linux-*) echo "Linux ${version#linux-}" ;;
        *) echo "Linux $version" ;;
    esac
}

boot_kernels() {
    local file
    local name

    for file in "$ROOT"/boot/vmlinuz-* "$ROOT"/boot/vmlinux-* "$ROOT"/boot/kernel-*; do
        name=$(basename "$file")

        case "$name" in
            *.old|*.sig|*.sha256|*.hmac) continue ;;
        esac

        if [ -f "$file" ]; then
            echo "$name"
        fi
    done
}

# Named kernels (Arch: linux, linux-lts) first with "linux" on top, then
# numbered kernels newest first.
sorted_kernels() {
    local name
    local version

    boot_kernels | while read -r name; do
        version=$(kernel_name "$name")
        case "$version" in
            [0-9]*) printf '2|%s|%s\n' "$version" "$name" ;;
            linux) printf '0|%s|%s\n' "$version" "$name" ;;
            *) printf '1|%s|%s\n' "$version" "$name" ;;
        esac
    done | sort -t'|' -k1,1n -k2,2Vr | cut -d'|' -f3
}

find_initrd() {
    local version
    local candidate

    for version in "$@"; do
        for candidate in "initrd.img-$version" "initrd-$version.img" "initrd-$version.gz" \
                         "initrd-$version" "initramfs-$version.img" \
                         "initramfs-genkernel-$version" "initramfs-$version"; do
            if [ -f "$ROOT/boot/$candidate" ]; then
                echo "$ROOT/boot/$candidate"
                return 0
            fi
        done
    done

    return 1
}

# "<kernel file>|<normal id>|<recovery id>" for every kernel in grub.cfg, so the
# ids match exactly what this distribution's GRUB generated.
grub_cfg_ids() {
    local config="$ROOT$GRUB_CFG"

    if [ ! -f "$config" ]; then
        return 0
    fi

    awk '
        function quoted_id(line,   rest) {
            if (match(line, /\$menuentry_id_option +\x27[^\x27]+\x27/)) {
                rest = substr(line, RSTART, RLENGTH)
                sub(/^[^\x27]*\x27/, "", rest)
                sub(/\x27$/, "", rest)
                return rest
            }
            return ""
        }
        /^[ \t]*submenu / { submenu = quoted_id($0); depth = 1; next }
        /^[ \t]*menuentry / {
            entry = quoted_id($0)
            title = $0
            if (submenu != "" && depth > 0) {
                entry = submenu ">" entry
            }
            next
        }
        /^[ \t]*linux(efi)?[ \t]/ && entry != "" {
            file = $2
            sub(/.*\//, "", file)
            if (title ~ /recovery/ || entry ~ /recovery/) {
                if (!(file in recovery)) recovery[file] = entry
            } else if (submenu != "" && entry ~ />/) {
                if (!(file in normal)) normal[file] = entry
            } else if (!(file in top)) {
                top[file] = entry
            }
            entry = ""
        }
        /^}/ { if (depth > 0) { depth = 0; submenu = "" } }
        END {
            for (file in top) if (!(file in normal)) normal[file] = top[file]
            for (file in normal) print file "|" normal[file] "|" recovery[file]
        }
    ' "$config"
}

grub_id_for() {
    local name=$1
    local column=$2

    echo "$GRUB_IDS" | awk -F'|' -v name="$name" -v n="$column" '$1 == name { print $n; exit }'
}

microcode_images() {
    local image

    for image in $MICROCODE_IMAGES; do
        if [ -f "$ROOT/boot/$image" ]; then
            echo "$ROOT/boot/$image"
        fi
    done
}

grub_root_uuid() {
    if [ -n "$GRUB_PROBE" ]; then
        "$GRUB_PROBE" --target=fs_uuid / 2>/dev/null || true
    fi
}

list_scanned_kernels() {
    local uuid=""
    local prefix=""
    local cmdline
    local name
    local version
    local kernel
    local initrd
    local parts
    local entry
    local recovery
    local direct
    local first=1

    load_grub_defaults
    cmdline=$(direct_boot_cmdline)

    if [ -z "$cmdline" ]; then
        disable_direct_boot "the kernel command line could not be read from /proc/cmdline"
    fi

    if [ "$BOOTLOADER" = grub ]; then
        echo "@oneshot grub"
        uuid=$(grub_root_uuid)
        prefix="gnulinux-advanced-$uuid>"
        case "$GRUB_DISABLE_SUBMENU" in
            y|true) prefix="" ;;
        esac
    else
        echo "@oneshot none"
    fi

    GRUB_IDS=$(grub_cfg_ids)

    for name in $(sorted_kernels); do
        version=$(kernel_name "$name")
        kernel="$ROOT/boot/$name"
        initrd=$(find_initrd "$version" "$(grub_version "$name")" || true)
        recovery=""

        if [ "$BOOTLOADER" = grub ]; then
            local normal_id
            local recovery_id
            local grub_name

            grub_name=$(grub_version "$name")
            normal_id=$(grub_id_for "$name" 2)
            recovery_id=$(grub_id_for "$name" 3)

            if [ -z "$normal_id" ]; then
                normal_id="${prefix}gnulinux-$grub_name-advanced-$uuid"
                recovery_id="${prefix}gnulinux-$grub_name-recovery-$uuid"
                if [ "$GRUB_DISABLE_RECOVERY" = "true" ]; then
                    recovery_id=""
                fi
            fi

            entry="$(kernel_title "$version")|$normal_id"
            if [ -n "$recovery_id" ]; then
                recovery="$(kernel_title "$version") (recovery)|$recovery_id"
            fi
        else
            entry="$(kernel_title "$version")|$name"
        fi

        if [ -n "$initrd" ] && [ -n "$cmdline" ]; then
            # shellcheck disable=SC2046
            set -- $(microcode_images) "$initrd"
            parts=$(esp_paths "$kernel" "$@" || true)

            if [ -n "$parts" ] && boots_in_place "$kernel"; then
                direct=$(esp_path "$kernel")
                entry="$entry$(direct_fields "$direct" "${parts#*,}" "$cmdline")"
            elif [ "$first" = 1 ] && copy_for_direct_boot "$kernel" "$@"; then
                entry="$entry$(copied_fields "$cmdline")"
            fi
        fi

        first=0
        echo "$entry"

        if [ -n "$recovery" ]; then
            echo "$recovery"
        fi
    done

    return 0
}