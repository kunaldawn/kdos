#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   testing/docscheck.sh — the books' own authoring check
#
#     docscheck.sh [--book docs/kdos|docs/kfs]... [PAGE]...
#
# Dead relative links, historical phrasing, and pages that break their book's
# page contract. It is the mechanical half of hard rule 2 — write the
# constraint, never the changelog — and it lives HERE, tracked, because a
# check that only ever existed under build/ is a check `make clean` deletes
# and no clone has.
#
# With no argument it checks every page of both books, docs/kdos and
# docs/kfs, plus README.md. --book narrows that to one book (repeatable), and
# README.md is then left out. A PAGE checks only that page's links, phrasing
# and contract, and the whole-book checks below are skipped.
#
# THE PAGE CONTRACT, in either book: the first line is a `# ` title and the
# page has a `## See also`. A whole-book run also refuses a page its book's
# README.md does not name (UNLISTED), so no chapter is unreachable from the
# contents. In docs/kfs the contents is written by testing/booknav.py, so an
# UNLISTED page there is a page booknav has not been run over.
#
# THE WIKI SKELETON, docs/kfs only: a topic page of the system administration
# part (the part directory named *-wiki, every page but its intro.md) has
# exactly these `## ` headings, in this order — Installation, Configuration,
# Usage, Tips, Troubleshooting, Known gaps, See also — so a reader finds the
# same thing in the same place on every page. Deeper headings are free.
#
# THE PORT COUNTS are docs/kdos only: testing/bookcounts.py recomputes every
# exact count on the three pages that state one, on a whole-book run that
# includes docs/kdos. docs/kfs states no count by hand; its package pages
# are generated.
#
# Anchors are testing/anchorcheck.py's and the chapter footers
# testing/booknav.py's; `make check` runs all three.

set -u
cd "$(git rev-parse --show-toplevel)" || exit 2

# Phrases that are almost always a record of the past rather than a statement
# of the present. This catches the common cases; it cannot catch a paragraph
# that narrates history in its own words, so read what you write.
BAD='used to be|used to have|it used to|which used to|no longer|now finally|was broken|cost a debug cycle|we decided|it turns out|previously,|historically,|in the past'

WIKI_SKELETON='Installation|Configuration|Usage|Tips|Troubleshooting|Known gaps|See also'

rc=0
books=()
files=()
while [ "$#" -gt 0 ]; do
    case "$1" in
        --book)
            [ "$#" -ge 2 ] || { echo "docscheck: --book needs docs/kdos or docs/kfs" >&2; exit 2; }
            b=${2%/}
            case "$b" in
                docs/kdos|docs/kfs) books+=("$b") ;;
                *) echo "docscheck: --book $2: not docs/kdos or docs/kfs" >&2; exit 2 ;;
            esac
            shift 2 ;;
        --book=*)
            set -- --book "${1#--book=}" "${@:2}" ;;
        *)  files+=("$1"); shift ;;
    esac
done

whole=0
if [ "${#files[@]}" -eq 0 ]; then
    whole=1
    if [ "${#books[@]}" -eq 0 ]; then
        books=(docs/kdos docs/kfs)
        readme=1
    else
        readme=0
    fi
    mapfile -t files < <(for b in "${books[@]}"; do
                             find "$b" -name '*.md' -not -path 'docs/kfs/data/*' | LC_ALL=C sort
                         done
                         [ "$readme" = 1 ] && echo README.md)
fi

# The `## ` headings of a page outside its code fences, one per line.
h2_of() {
    awk '/^[[:space:]]*(```|~~~)/ { f = !f; next }
         !f && /^## / { sub(/^## +/, ""); sub(/[[:space:]]+$/, ""); print }' "$1"
}

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
        docs/kdos/*|docs/kfs/*)
            head -1 "$f" | grep -q '^# ' || { echo "NOTITLE   $f"; rc=1; }
            grep -q '^## See also' "$f" || { echo "NOSEEALSO $f"; rc=1; }
            ;;
    esac

    case "$f" in
        docs/kfs/[0-9][0-9]-wiki/intro.md) ;;
        docs/kfs/[0-9][0-9]-wiki/*.md)
            got=$(h2_of "$f" | paste -sd'|')
            if [ "$got" != "$WIKI_SKELETON" ]; then
                echo "WIKISKEL  $f"
                echo "  want: ${WIKI_SKELETON//|/ · }"
                echo "  have: ${got//|/ · }"
                rc=1
            fi
            ;;
    esac
done

# Every book page reachable from its book's table of contents.
if [ "$whole" = 1 ]; then
    for b in "${books[@]}"; do
        while read -r page; do
            rel=${page#"$b"/}
            [ "$rel" = "README.md" ] && continue
            grep -qF "($rel" "$b/README.md" || { echo "UNLISTED  $page"; rc=1; }
        done < <(find "$b" -name '*.md' -not -path 'docs/kfs/data/*' | LC_ALL=C sort)
    done
fi

# The port counts: exact recipe, shelf and package-list figures live only on
# status.md, ports-catalogue.md and packaging.md, and testing/bookcounts.py
# recomputes every one from the tree. The vendoring counts on other pages are
# not recomputed.
if [ "$whole" = 1 ] && printf '%s\n' "${books[@]}" | grep -qx docs/kdos; then
    python3 testing/bookcounts.py || rc=1
fi

[ $rc -eq 0 ] && echo "docscheck: ok"
exit $rc
