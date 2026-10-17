#!/usr/bin/env python3
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   recipemeta — the metadata keys every recipe carries, against the book
# ---------------------------------
#
# Run from the repository root. Every kpkgbuild under ports/core and src/ is
# read the way kpkg info, the version checker and kdos cve read it: the FIRST
# line of a key, its value taken literally, no helper expanded. Four rules:
#
#   meta      description, homepage and license are present and not empty
#   license   license is an SPDX expression: identifiers (LicenseRef-… and a
#             trailing + included) joined by AND, OR and WITH, in parentheses
#             as needed. The identifiers are not checked against the SPDX
#             list, which this tree does not carry.
#   version   the first source does not spell the recipe's version literally.
#             A literal version keeps naming the old release after a bump;
#             VERSION_LITERAL_OK names the recipes that fetch several versions
#             on purpose.
#   catalogue the description is exactly the third cell of the port's row in
#             docs/kdos/06-reference/ports-catalogue.md, every port has one
#             row and every row names a port. The recipe is the source of
#             truth; the book repeats it.
#
#   python3 testing/recipemeta.py           every finding, one line each
#   python3 testing/recipemeta.py --brief   per rule: a count and the first
#                                           names, as testing/preflight.sh
#                                           prints them
#
# Exit status 1 when anything is found.

import os
import re
import sys

CATALOGUE = "docs/kdos/06-reference/ports-catalogue.md"
VERSION_LITERAL_OK = {"docbook-xml"}
# The catalogue's sections that hold one row per port, `| name | version |
# description | phase |`; its other tables repeat ports or name phases.
PORT_SECTIONS = {"## The shelves", "## KDOS's own programs"}


def recipes():
    out = {}
    for top in ("ports/core", "src"):
        if not os.path.isdir(top):
            continue
        for g in sorted(os.listdir(top)):
            d = os.path.join(top, g)
            if not os.path.isdir(d):
                continue
            for n in sorted(os.listdir(d)):
                k = os.path.join(d, n, "kpkgbuild")
                if os.path.isfile(k):
                    out.setdefault(n, k)
    return out


KEY = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*)[ \t]*=[ \t]?(.*)$")


def first_keys(path):
    keys = {}
    with open(path, errors="replace") as f:
        for line in f:
            if line.lstrip().startswith("#"):
                continue
            m = KEY.match(line.rstrip("\n"))
            if m and m.group(1) not in keys:
                keys[m.group(1)] = m.group(2).strip()
    return keys


TOKEN = re.compile(r"\s*(\(|\)|[A-Za-z0-9.+:-]+)")


def spdx_ok(expr):
    toks = []
    pos = 0
    expr = expr.strip()
    while pos < len(expr):
        m = TOKEN.match(expr, pos)
        if not m:
            return False
        toks.append(m.group(1))
        pos = m.end()
    ops = {"AND", "OR", "WITH", "and", "or", "with"}
    i = 0

    def term():
        nonlocal i
        if i >= len(toks):
            return False
        if toks[i] == "(":
            i += 1
            if not expression() or i >= len(toks) or toks[i] != ")":
                return False
            i += 1
            return True
        if toks[i] in ops or toks[i] == ")":
            return False
        i += 1
        if i < len(toks) and toks[i] in ("WITH", "with"):
            i += 1
            if i >= len(toks) or toks[i] in ops or toks[i] in "()":
                return False
            i += 1
        return True

    def expression():
        nonlocal i
        if not term():
            return False
        while i < len(toks) and toks[i] in ("AND", "OR", "and", "or"):
            i += 1
            if not term():
                return False
        return True

    return bool(toks) and expression() and i == len(toks)


def catalogue_rows():
    rows = {}
    dupes = []
    if not os.path.isfile(CATALOGUE):
        return rows, dupes
    row = re.compile(r"^\| `([^`]+)` \| ([^|]*) \| (.*) \| ([^|]*) \|\s*$")
    inside = False
    with open(CATALOGUE) as f:
        for line in f:
            if line.startswith("## "):
                inside = line.strip() in PORT_SECTIONS
                continue
            m = row.match(line) if inside else None
            if not m:
                continue
            n = m.group(1)
            if n in rows:
                dupes.append(n)
            rows[n] = m.group(3).strip()
    return rows, dupes


def main():
    brief = "--brief" in sys.argv[1:]
    found = {"meta": [], "license": [], "version": [], "catalogue": []}
    recs = recipes()
    rows, dupes = catalogue_rows()
    for n, path in sorted(recs.items()):
        k = first_keys(path)
        for key in ("description", "homepage", "license"):
            if not k.get(key):
                found["meta"].append("%s: no %s" % (n, key))
        lic = k.get("license")
        if lic and not spdx_ok(lic):
            found["license"].append("%s: '%s' is not an SPDX expression"
                                    % (n, lic))
        ver = k.get("version", "")
        src = k.get("source", "")
        if (n not in VERSION_LITERAL_OK and src and ver and "$" not in ver
                and len(ver) > 1 and re.search(
                    r"(?<![0-9A-Za-z])" + re.escape(ver) + r"(?![0-9])", src)):
            found["version"].append("%s: first source spells %s for $version"
                                    % (n, ver))
        desc = k.get("description")
        if n not in rows:
            found["catalogue"].append("%s: no row in the catalogue" % n)
        elif desc is not None and rows[n] != desc:
            found["catalogue"].append("%s: recipe '%s', catalogue '%s'"
                                      % (n, desc, rows[n]))
    if os.path.isfile(CATALOGUE):
        for n in sorted(set(rows) - set(recs)):
            found["catalogue"].append("%s: a catalogue row with no port" % n)
        for n in sorted(set(dupes)):
            found["catalogue"].append("%s: two catalogue rows" % n)
    else:
        found["catalogue"].append("%s is missing" % CATALOGUE)

    words = {
        "meta": "recipes lack description, homepage or license",
        "license": "license values are not SPDX expressions",
        "version": "first sources spell the version literally",
        "catalogue": "descriptions or rows differ from ports-catalogue.md",
    }
    bad = 0
    for rule, items in found.items():
        bad += len(items)
        if brief:
            if items:
                names = [x.split(":", 1)[0] for x in items]
                more = " …" if len(names) > 8 else ""
                print("FAIL\t%s\t%d %s: %s%s" % (rule, len(items), words[rule],
                                                ", ".join(names[:8]), more))
            else:
                print("ok\t%s\tnone" % rule)
        else:
            for x in items:
                print("%s: %s" % (rule, x))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
