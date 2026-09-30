#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   testing/debuginfo.sh — which installed files carry DWARF
#
# kpkg strips nothing: a port ships debug information exactly when its
# compile or link flags produce it. This lists every ELF under usr/ of a root
# (default build/fs) that has a .debug_info section, grouped by the package
# whose manifest in var/lib/kpkg/db names it, largest package first, with the
# file's size and its .debug_info size in bytes.
#
# Left out, because their debug information is not a flag a recipe sets or
# they are not programs this system runs: kernel modules, firmware, the
# cross-target sysroots (arm-none-eabi, avr, riscv*-elf) and BPF objects.
#
# Read-only. Exit status 1 when anything is reported, 0 when nothing is.
#
#   testing/debuginfo.sh [ROOT]

set -u
root=${1:-build/fs}
[ -d "$root/usr" ] || { echo "debuginfo: $root/usr is not a directory" >&2; exit 2; }
command -v readelf >/dev/null || { echo "debuginfo: readelf is required" >&2; exit 2; }

sp=$(mktemp -d)
trap 'rm -rf "$sp"' EXIT

# path<TAB>package for every manifest line; a manifest is the package's
# `tar -tf` listing, so a leading ./ is dropped to match the find paths. The
# first line is a placeholder: the join below reads its first file up to the
# second's first line, and an empty first file would take the hits for owners.
db="$root/var/lib/kpkg/db"
printf '\t\n' > "$sp/owners"
if [ -d "$db" ]; then
    for m in "$db"/*; do
        [ -f "$m" ] || continue
        awk -v p="${m##*/}" '{ sub(/^\.\//, ""); sub(/\/$/, ""); print $0 "\t" p }' "$m"
    done > "$sp/owners"
fi

(cd "$root" && find usr -type f \
    -not -path 'usr/lib/modules/*' \
    -not -path '*/firmware/*' \
    -not -path '*/arm-none-eabi/*' \
    -not -path '*/avr/*' \
    -not -path '*/riscv*-elf/*' \
    -not -name '*.bpf.o' -print) |
while IFS= read -r f; do
    [ "$(head -c4 "$root/$f" 2>/dev/null | od -An -tx1 | tr -d ' \n')" = 7f454c46 ] || continue
    dbg=$(readelf -SW "$root/$f" 2>/dev/null \
          | awk '{ for (i = 1; i <= NF; i++) if ($i == ".debug_info") { print $(i + 4); exit } }')
    [ -n "$dbg" ] || continue
    printf '%s\t%s\t%s\n' "$f" "$(stat -c %s "$root/$f")" "$((16#$dbg))"
done > "$sp/hits"

[ -s "$sp/hits" ] || { echo "no ELF under $root/usr carries .debug_info"; exit 0; }

awk -F'\t' '
    FNR == NR { own[$1] = $2; next }
    {
        p = ($1 in own) ? own[$1] : "(unowned)"
        tot[p] += $2; dbg[p] += $3; n[p]++
        line[p] = line[p] sprintf("    %12d %12d  %s\n", $2, $3, $1)
        T += $2; D += $3; N++
    }
    END {
        for (p in tot) printf "%d\t%s\n", tot[p], p | "sort -rn > \"'"$sp"'/order\""
        close("sort -rn > \"'"$sp"'/order\"")
        while ((getline l < "'"$sp"'/order") > 0) {
            split(l, a, "\t"); p = a[2]
            printf "%s  (%d files, %d bytes, %d of .debug_info)\n%s", p, n[p], tot[p], dbg[p], line[p]
        }
        printf "\n%d files, %d bytes, %d of .debug_info\n", N, T, D
    }
' "$sp/owners" "$sp/hits"
exit 1
