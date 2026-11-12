# Add or update this OS's line in aarambh.conf. Sourced by setup.sh.

# distro_field <column> <ids...>: first match in distros.conf.
distro_field() {
    local field=$1
    local distros="$ESP_DIR/distros.conf"
    local id

    shift

    if [ ! -f "$distros" ]; then
        return 1
    fi

    for id in "$@"; do
        local value

        value=$(awk -F'|' -v id="$id" -v n="$field" '$1 == id { print $n; exit }' "$distros")
        if [ -n "$value" ]; then
            echo "$value"
            return 0
        fi
    done

    return 1
}

# Logos, backgrounds and titles use the exact ID only, so a derivative never
# shows its parent's branding. Missing files fall back to a letter and the
# accent color at boot.
asset_for() {
    echo "$1/$OS_ID.$2"
}

menu_title() {
    distro_field 2 "$OS_ID" || echo "$OS_NAME"
}

menu_line() {
    local loader=$1
    local accent

    accent=$(distro_field 3 $(os_ids) || true)

    printf 'entry %s | %s | %s | %s | %s' \
        "$(menu_title)" "$loader" "$(asset_for logos png)" "$(asset_for backgrounds jpg)" "$KEY"

    if [ -n "$accent" ]; then
        printf ' | %s' "$accent"
    fi

    echo
}

# Replace the line with this key, or insert it before the first action entry.
register_menu_entry() {
    local line
    local config="$ESP_DIR/aarambh.conf"

    line=$(menu_line "$1")
    log "menu entry: $line"

    if [ "$DRY_RUN" = 1 ]; then
        echo "  would update: $config"
        return 0
    fi

    ENTRY_KEY="$KEY" ENTRY_LINE="$line" awk '
        BEGIN {
            key = ENVIRON["ENTRY_KEY"]
            line = ENVIRON["ENTRY_LINE"]
        }
        function entry_key(text,   fields) {
            split(text, fields, "|")
            gsub(/^[ \t]+|[ \t]+$/, "", fields[5])
            return fields[5]
        }
        FNR == NR {
            if ($0 ~ /^entry[ \t]/ && entry_key($0) == key) {
                exists = 1
            }
            next
        }
        /^entry[ \t]/ && exists && entry_key($0) == key {
            if (!done) {
                print line
                done = 1
            }
            next
        }
        /^entry[ \t]/ && !exists && !done && $0 ~ /\|[ \t]*@/ {
            print line
            done = 1
        }
        { print }
        END {
            if (!done) {
                print line
            }
        }
    ' "$config" "$config" | replace_file "$config" \
        || die "could not update $config (is the EFI partition full or read-only?)"
}
