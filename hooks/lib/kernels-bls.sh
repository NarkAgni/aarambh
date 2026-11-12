# Kernel list from BootLoaderSpec entries: Fedora, RHEL and friends (GRUB with
# BLS) and every systemd-boot system. Sourced.

bls_field() {
    sed -n "s/^$1[[:space:]]\\+//p" "$2" | head -n1
}

bls_all_fields() {
    sed -n "s/^$1[[:space:]]\\+//p" "$2"
}

bls_entry_dirs() {
    if [ "$BOOTLOADER" = systemd-boot ] && [ -d "$ESP_ROOT/loader/entries" ]; then
        echo "$ESP_ROOT/loader/entries"
    fi

    if [ -d "$ROOT/boot/loader/entries" ]; then
        if [ "$(realpath "$ROOT/boot/loader/entries")" != "$(realpath -m "$ESP_ROOT/loader/entries")" ]; then
            echo "$ROOT/boot/loader/entries"
        fi
    fi
}

machine_id() {
    cat "$ROOT/etc/machine-id" 2>/dev/null || true
}

root_device() {
    sed -n 's/.*\broot=\([^ ]*\).*/\1/p' "$ROOT/proc/cmdline" 2>/dev/null || true
}

# The machine id an entry was written for: its machine-id field, or the id
# kernel-install puts in front of the file name.
entry_machine_id() {
    local file=$1
    local id
    local name

    id=$(bls_field machine-id "$file")
    name=$(basename "$file")

    if [ -z "$id" ] && [[ "$name" =~ ^([0-9a-f]{32})- ]]; then
        id=${BASH_REMATCH[1]}
    fi

    echo "$id"
}

# Succeeds only when the entry's options hold exactly this root= argument:
# /dev/sda1 must not match /dev/sda10.
boots_root() {
    local file=$1
    local root=$2
    local token

    for token in $(bls_all_fields options "$file"); do
        if [ "$token" = "root=$root" ]; then
            return 0
        fi
    done

    return 1
}

# On a shared ESP, keep only the entries that belong to this installation.
# A machine id decides when both sides have one; only an entry without one is
# matched by its root= argument.
belongs_to_this_os() {
    local file=$1
    local id
    local entry_id
    local root

    if [ "$BOOTLOADER" != systemd-boot ]; then
        return 0
    fi

    id=$(machine_id)
    entry_id=$(entry_machine_id "$file")
    root=$(root_device)

    if [ -n "$id" ]; then
        case "$(basename "$file")" in
            "$id"-*) return 0 ;;
        esac

        if [ -n "$entry_id" ]; then
            [ "$entry_id" = "$id" ]
            return
        fi
    fi

    [ -n "$root" ] && boots_root "$file" "$root"
}

entries_by_version() {
    local dir
    local file
    local version

    for dir in $(bls_entry_dirs); do
        for file in "$dir"/*.conf; do
            if [ -f "$file" ] && belongs_to_this_os "$file"; then
                version=$(bls_field version "$file")
                if [ -z "$version" ]; then
                    version=$(basename "$file" .conf)
                fi
                echo "$version|$file"
            fi
        done
    done | sort -t'|' -k1,1 -V -r
}

# Resolve a path from an entry against the partition that holds the entry.
bls_file() {
    local entry=$1
    local path=$2
    local base
    local candidate

    base=$(dirname "$(dirname "$(dirname "$entry")")")

    for candidate in "$base$path" "$ROOT$path" "$ROOT/boot$path"; do
        if [ -f "$candidate" ]; then
            echo "$candidate"
            return 0
        fi
    done

    return 1
}

grub_env_value() {
    { grub2-editenv list 2>/dev/null || grub-editenv list 2>/dev/null || true; } \
        | sed -n "s/^$1=//p" | head -n1
}

# Expand $name and ${name} the way GRUB's blscfg does: values come from
# grubenv and an unset variable is empty. Nothing is evaluated by the shell.
grub_expand() {
    local text=$1
    local result=""
    local name

    while [[ "$text" == *'$'* ]]; do
        result+=${text%%'$'*}
        text=${text#*'$'}

        if [[ "$text" =~ ^\{([A-Za-z_][A-Za-z0-9_]*)\}(.*)$ ]] \
            || [[ "$text" =~ ^([A-Za-z_][A-Za-z0-9_]*)(.*)$ ]]; then
            name=${BASH_REMATCH[1]}
            text=${BASH_REMATCH[2]}
            result+=$(grub_env_value "$name")
        else
            result+='$'
        fi
    done

    printf '%s' "$result$text"
}

# systemd-boot does not expand variables, GRUB does.
bls_expand() {
    if [ "$BOOTLOADER" = grub ] && [[ "$1" == *'$'* ]]; then
        grub_expand "$1"
    else
        printf '%s' "$1"
    fi
}

uses_kernelopts() {
    case "$1" in
        *'$kernelopts'*|*'${kernelopts}'*) [ "$BOOTLOADER" = grub ] ;;
        *) return 1 ;;
    esac
}

# The files of every initrd line, in order. Fails when one that the entry
# names is missing: without it the kernel may not find its root filesystem.
bls_initrds() {
    local entry=$1
    local paths
    local path
    local file
    local files=""

    paths=$(bls_expand "$(bls_all_fields initrd "$entry" | tr '\n' ' ')")

    for path in $paths; do
        file=$(bls_file "$entry" "$path") || return 1
        files="$files$file"$'\n'
    done

    printf '%s' "$files"
}

# The options of an entry on one line. Fails when they rely on $kernelopts
# and grubenv has none, because root= would be missing.
bls_options() {
    local options

    options=$(bls_all_fields options "$1" | tr '\n' ' ')

    if uses_kernelopts "$options" && [ -z "$(grub_env_value kernelopts)" ]; then
        return 1
    fi

    printf '%s\n' "$(bls_expand "$options")" | sed 's/[[:space:]]\+/ /g; s/^ //; s/ $//'
}

bls_title() {
    local entry=$1
    local version=$2

    case "$version" in
        *rescue*) echo "Rescue mode" ;;
        [0-9]*) echo "Linux $version" ;;
        *) bls_field title "$entry" ;;
    esac
}

bls_id() {
    if [ "$BOOTLOADER" = systemd-boot ]; then
        basename "$1"
    else
        basename "$1" .conf
    fi
}

# The direct-boot fields of one entry, or nothing when it has to start through
# the OS's own loader: kernel not found, an initrd it names missing, or
# options that cannot be worked out.
bls_direct_fields() {
    local entry=$1
    local version=$2
    local newest=$3
    local kernel
    local initrds
    local options
    local parts

    kernel=$(bls_file "$entry" "$(bls_field linux "$entry")") || return 0

    if ! initrds=$(bls_initrds "$entry"); then
        disable_direct_boot "$(basename "$entry") names an initrd that does not exist"
        return 0
    fi

    if ! options=$(bls_options "$entry"); then
        disable_direct_boot "$(basename "$entry") uses \$kernelopts, which grubenv does not set"
        return 0
    fi

    # shellcheck disable=SC2086
    parts=$(esp_paths $initrds || true)

    if esp_path "$kernel" >/dev/null && boots_in_place "$kernel" \
        && { [ -z "$initrds" ] || [ -n "$parts" ]; }; then
        direct_fields "$(esp_path "$kernel")" "$parts" "$options"
    elif [ "$newest" = 1 ] && [ -n "$initrds" ] && [[ "$version" != *rescue* ]]; then
        # shellcheck disable=SC2086
        if copy_for_direct_boot "$kernel" $initrds; then
            copied_fields "$options"
        fi
    fi
}

list_bls_kernels() {
    local version
    local entry
    local first=1

    if [ "$BOOTLOADER" = systemd-boot ]; then
        echo "@oneshot systemd-boot"
    else
        echo "@oneshot grub"
    fi

    entries_by_version | while IFS='|' read -r version entry; do
        echo "$(bls_title "$entry" "$version")|$(bls_id "$entry")$(bls_direct_fields "$entry" "$version" "$first")"

        if [[ "$version" != *rescue* ]] && bls_file "$entry" "$(bls_field linux "$entry")" >/dev/null; then
            first=0
        fi
    done
}