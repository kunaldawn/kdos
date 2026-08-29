#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------
#
# Read what `testing/appsweep.sh` brought back and say what it means.
# HOST-SIDE: it needs ImageMagick, which the target does not have and does not
# want.
#
#   appreport.sh <scratch.img> [outdir]
#
# TWO THINGS IT DOES THAT THE GUEST CANNOT:
#
#   - THE BLANK CHECK. A window that mapped and painted nothing passes every
#     test the guest can run — the compositor reported it, grim wrote a file,
#     the bytes are non-zero. Only the PIXELS say otherwise, and a PNG of one
#     flat colour still compresses to a few KB. `-format %k` is the unique
#     colour count; a screenshot of a real application is in the thousands and
#     an empty surface is in single figures.
#
#   - THE CONTACT SHEET. 183 windows is not something anybody reviews one file
#     at a time.
#
# A PERCENTAGE WITHOUT ITS SKIPS IS A NUMBER DESIGNED TO LOOK GOOD, so the
# three states are always printed together and the skip reasons are printed in
# full rather than counted.

set -u

IMG="${1:?usage: appreport.sh <scratch.img> [outdir]}"
OUT="${2:-build/appreport}"
# Below this many distinct colours a screenshot is an empty surface, not an
# application. The desktop's own wallpaper and panel are already well past it,
# which is why the threshold can be this low and still mean something.
BLANK_COLOURS=${BLANK_COLOURS:-24}

command -v convert >/dev/null || { echo "needs ImageMagick"; exit 2; }

rm -rf "$OUT"; mkdir -p "$OUT"
tar xf "$IMG" -C "$OUT" 2>/dev/null || { echo "no tar on $IMG"; exit 2; }
SW="$OUT/sweep"
TSV="$SW/results.tsv"
[ -f "$TSV" ] || { echo "no results.tsv in the tar"; exit 2; }

# ── the blank check, which is the whole reason this runs on the host ────────
echo "== blank-frame check"
: > "$OUT/blank.txt"
for png in "$SW"/*.png; do
    [ -e "$png" ] || continue
    id=$(basename "$png" .png)
    k=$(convert "$png" -format %k info: 2>/dev/null || echo 0)
    if [ "${k:-0}" -lt "$BLANK_COLOURS" ]; then
        echo "$id	$k" >> "$OUT/blank.txt"
    fi
done
nblank=$(wc -l < "$OUT/blank.txt")
echo "   $nblank shot(s) under $BLANK_COLOURS distinct colours"

# ── the table ──────────────────────────────────────────────────────────────
echo
awk -F'\t' -v blank="$OUT/blank.txt" '
    BEGIN {
        while ((getline l < blank) > 0) { split(l, b, "\t"); isblank[b[1]] = b[2] }
    }
    NR == 1 { next }
    {
        id = $1; st = $2; why = $3
        if (st == "ok" && id in isblank) { st = "fail"; why = "blank (" isblank[id] " colours)" }
        n[st]++
        if (st != "ok") printf "  %-28s %-5s %s\n", id, st, why
        tot++
    }
    END {
        printf "\n  %d app(s): %d ok, %d fail, %d skip\n",
               tot, n["ok"]+0, n["fail"]+0, n["skip"]+0
        if (tot > 0)
            printf "  %.0f%% of ATTEMPTED apps mapped a window and drew something;\n",
                   (n["ok"]+0) * 100.0 / tot
        printf "  the skips are not passes and are listed above with their reason.\n"
    }
' "$TSV"

# ── the contact sheet ──────────────────────────────────────────────────────
shots=$(ls "$SW"/*.png 2>/dev/null | wc -l)
if [ "$shots" -gt 0 ]; then
    echo
    echo "== contact sheet ($shots shots)"
    montage "$SW"/*.png -label '%t' -tile 6x -geometry 260x165+3+3 \
            -background '#04120a' -fill '#39ff14' -pointsize 11 \
            "$OUT/contact.png" 2>/dev/null \
        && echo "   $OUT/contact.png" \
        || echo "   montage failed"
fi
echo
echo "== $OUT"
