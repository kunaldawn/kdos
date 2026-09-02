#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   testing/docscheck.sh — the book's own authoring check
#
# Dead relative links, historical phrasing, and pages that break the page
# contract. It is the mechanical half of hard rule 2 — write the constraint,
# never the changelog — and it lives HERE, tracked, because a check that only
# ever existed under build/ is a check `make clean` deletes and no clone has.
#
# With no argument it checks every page under docs/kdos plus README.md; give it
# paths to check only those.

set -u
cd "$(git rev-parse --show-toplevel)" || exit 2

# Phrases that are almost always a record of the past rather than a statement
# of the present. This catches the common cases; it cannot catch a paragraph
# that narrates history in its own words, so read what you write.
BAD='used to be|used to have|it used to|which used to|no longer|now finally|was broken|cost a debug cycle|we decided|it turns out|previously,|historically,|in the past'

rc=0

if [ "$#" -gt 0 ]; then
    files=("$@")
else
    mapfile -t files < <(find docs/kdos -name '*.md' | sort; echo README.md)
fi

for f in "${files[@]}"; do
    if [ ! -f "$f" ]; then echo "MISSING   $f"; rc=1; continue; fi
    dir=$(dirname "$f")

    # Dead relative links. Skips URLs and pure anchors; strips any #fragment.
    dead=$(grep -oE '\]\([^)]+\)' "$f" | sed -E 's/^\]\(//; s/\)$//' | while read -r link; do
        case "$link" in http*|mailto:*|'#'*) continue ;; esac
        target=${link%%#*}
        [ -z "$target" ] && continue
        if [ ! -e "$dir/$target" ] && [ ! -e "$target" ]; then echo "  -> $link"; fi
    done)
    if [ -n "$dead" ]; then echo "DEADLINK  $f"; echo "$dead"; rc=1; fi

    hist=$(grep -niE "$BAD" "$f" | head -5)
    if [ -n "$hist" ]; then echo "HISTORY   $f"; echo "$hist" | sed 's/^/  /'; rc=1; fi

    case "$f" in
        docs/kdos/*)
            head -1 "$f" | grep -q '^# ' || { echo "NOTITLE   $f"; rc=1; }
            grep -q '^## See also' "$f" || { echo "NOSEEALSO $f"; rc=1; }
            ;;
    esac
done

# Every book page reachable from the table of contents.
if [ "$#" -eq 0 ]; then
    while read -r page; do
        rel=${page#docs/kdos/}
        [ "$rel" = "README.md" ] && continue
        grep -q "$rel" docs/kdos/README.md || { echo "UNLISTED  $page"; rc=1; }
    done < <(find docs/kdos -name '*.md' | sort)
fi

[ $rc -eq 0 ] && echo "docscheck: ok"
exit $rc
