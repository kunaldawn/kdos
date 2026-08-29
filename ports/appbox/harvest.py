#!/usr/bin/env python3
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   ports/appbox/harvest.py — a pack's metadata, out of the layer it packs
# ---------------------------------
#
# HOST-ONLY, and it is python for one reason: an application's own description
# is in AppStream XML, THERE IS NO XML PARSER IN THIS TREE AND THERE IS NOT
# GOING TO BE ONE. The same split the three vendor.py scripts and genatlas.py
# already keep — the host reads the awkward format, and the TARGET only ever
# reads flat `key = value`.
#
# Run against a pack's diff directory, which is exactly what that stage added.
#
#     harvest.py <diffdir> <id> <kind> <parent> --out meta.txt --icon icon.png
#
# THE ICON IS THE ONLY IMAGE A PACK CARRIES, and it is UNTINTED — a phosphor
# Firefox mark is vandalism, which is the split `kdos-theme icons` already
# keeps. Screenshots are not harvested: they existed for a storefront that is
# not being built, and a picture of software already on the disk is not worth
# carrying.

import argparse
import os
import re
import struct
import sys
import zlib
import xml.etree.ElementTree as ET

DESKTOP_DIRS = ("usr/share/applications",)
METAINFO_DIRS = ("usr/share/metainfo", "usr/share/appdata")
ICON_ROOTS = ("usr/share/icons/hicolor", "usr/share/pixmaps")


def desktop_entries(root):
    """Every [Desktop Entry] this layer added, as dicts."""
    out = []
    for d in DESKTOP_DIRS:
        full = os.path.join(root, d)
        if not os.path.isdir(full):
            continue
        for name in sorted(os.listdir(full)):
            if not name.endswith(".desktop"):
                continue
            path = os.path.join(full, name)
            if not os.path.isfile(path):
                continue
            e, section = {}, None
            with open(path, encoding="utf-8", errors="replace") as fh:
                for line in fh:
                    line = line.rstrip("\n")
                    if line.startswith("["):
                        section = line
                        continue
                    if section != "[Desktop Entry]" or "=" not in line:
                        continue
                    k, v = line.split("=", 1)
                    # The localised keys are somebody else's business: this
                    # tree has no message catalogues for them to be read from.
                    if "[" in k:
                        continue
                    e[k.strip()] = v.strip()
            if e.get("Type", "Application") != "Application":
                continue
            if e.get("NoDisplay", "").lower() == "true":
                continue
            e["_id"] = name[: -len(".desktop")]
            out.append(e)
    return out


def component_rank(cid, want):
    """How well an AppStream component id names the pack we are building.

    THE SAME RULE THE DESKTOP ENTRIES GET, and it belongs here for the same
    reason: a stage's layer carries the metainfo of every package it pulled in,
    not just the application. Taking the first file alphabetically named
    `app.bcnc` and `app.digikam` both after libgphoto2, and `app.cantor` after
    the URW font set — a shop window describing somebody else's program.
    """
    cid = cid.lower()
    if cid.endswith(".desktop"):
        cid = cid[: -len(".desktop")]
    leaf = cid.rsplit(".", 1)[-1]
    if leaf == want or cid == want:
        return 0
    if leaf.startswith(want) or leaf.endswith(want):
        return 1
    if want in cid:
        return 2
    return 3


def metainfo(root, want=""):
    """summary, description and licence, from AppStream when there is any."""
    got = {}
    found = []
    for d in METAINFO_DIRS:
        full = os.path.join(root, d)
        if not os.path.isdir(full):
            continue
        for name in sorted(os.listdir(full)):
            if not name.endswith(".xml") and not name.endswith(".xml.in"):
                continue
            try:
                tree = ET.parse(os.path.join(full, name))
            except Exception:
                # A component this build cannot parse is ABSENT, not partial.
                continue
            r = tree.getroot()
            cid = ""
            for el in r.iter("id"):
                cid = " ".join((el.text or "").split())
                break
            # The filename is the tiebreak, so the order is still total and a
            # re-bake of an unchanged layer produces the same metadata.
            found.append((component_rank(cid, want) if want else 3, name, r))
    found.sort(key=lambda t: (t[0], t[1]))
    if found:
        got["_rank"] = found[0][0]
    for _rank, _name, r in found:
        def text(tag):
            for el in r.iter(tag):
                if el.get("{http://www.w3.org/XML/1998/namespace}lang"):
                    continue
                return " ".join((el.text or "").split())
            return ""
        if not got.get("summary"):
            got["summary"] = text("summary")
        if not got.get("licence"):
            got["licence"] = text("project_license")
        if not got.get("description"):
            paras = []
            for desc in r.iter("description"):
                if desc.get("{http://www.w3.org/XML/1998/namespace}lang"):
                    continue
                for p in desc.iter("p"):
                    if p.get("{http://www.w3.org/XML/1998/namespace}lang"):
                        continue
                    paras.append(" ".join((p.text or "").split()))
                break
            got["description"] = [p for p in paras if p]
    return got


def png_is_blank(data):
    """
    An icon that rasterises to nothing is DROPPED, not shipped — the genatlas
    lesson: an empty picture still takes the sprite slot and the caller never
    falls back to its glyph, which is how four Start menu rows silently lost
    their icon.

    Decoded here rather than with a library, because the only thing being asked
    is whether every pixel is transparent.
    """
    if len(data) < 8 or data[:8] != b"\x89PNG\r\n\x1a\n":
        return True
    pos, w, h, depth, ctype, idat = 8, 0, 0, 0, 0, b""
    while pos + 8 <= len(data):
        ln = struct.unpack(">I", data[pos:pos + 4])[0]
        typ = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + ln]
        if typ == b"IHDR":
            w, h, depth, ctype = struct.unpack(">IIBB", body[:10])
        elif typ == b"IDAT":
            idat += body
        elif typ == b"IEND":
            break
        pos += 12 + ln
    if not w or not h or not idat:
        return True
    if ctype not in (4, 6) or depth != 8:
        # No alpha channel at all: it cannot be fully transparent.
        return False
    try:
        raw = zlib.decompress(idat)
    except Exception:
        return True
    chan = 2 if ctype == 4 else 4
    stride = w * chan
    prev = bytearray(stride)
    at = 0
    for _ in range(h):
        if at + 1 + stride > len(raw):
            return True
        ft = raw[at]
        line = bytearray(raw[at + 1:at + 1 + stride])
        at += 1 + stride
        for i in range(stride):
            a = line[i - chan] if i >= chan else 0
            b = prev[i]
            c = prev[i - chan] if i >= chan else 0
            if ft == 1:
                line[i] = (line[i] + a) & 0xFF
            elif ft == 2:
                line[i] = (line[i] + b) & 0xFF
            elif ft == 3:
                line[i] = (line[i] + (a + b) // 2) & 0xFF
            elif ft == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 0xFF
        for i in range(chan - 1, stride, chan):
            if line[i]:
                return False
        prev = line
    return True


def find_icon(root, name):
    """The largest PNG this layer added for that icon name."""
    if not name:
        return None
    best, best_px = None, -1
    for r in ICON_ROOTS:
        full = os.path.join(root, r)
        for dirpath, _dirs, files in os.walk(full):
            for f in files:
                if f not in (name + ".png",):
                    continue
                m = re.search(r"/(\d+)x\1/", dirpath + "/")
                px = int(m.group(1)) if m else 0
                if px > best_px:
                    best_px, best = px, os.path.join(dirpath, f)
    return best


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("diffdir")
    ap.add_argument("id")
    ap.add_argument("kind")
    ap.add_argument("parent")
    ap.add_argument("--out", required=True)
    ap.add_argument("--icon")
    ap.add_argument("--version", default="1")
    ap.add_argument("--recommended", action="store_true")
    ap.add_argument("--env", action="append", default=[],
                    metavar="NAME=VALUE")
    ap.add_argument("--command", action="append", default=[],
                    metavar="NAME")
    ap.add_argument("--needs", action="append", default=[],
                    metavar="PACK", help="a data pack this app is useless without")
    ap.add_argument("--graft", action="append", default=[],
                    metavar="FROM TO", help="data pack: symlink under /usr/share")
    ap.add_argument("--boxgraft", action="append", default=[],
                    metavar="FROM TO", help="data pack: symlink a box can see")
    a = ap.parse_args()

    # ONLY AN APP ROW HAS AN APPLICATION'S IDENTITY. A runtime layer holds
    # whatever its handful of packages dragged in, and the AppStream scan takes
    # the first component that ships a file — so harvesting one would describe
    # the Qt runtime as "Video thumbnail generator using FFmpeg", which is
    # ffmpegthumbs. A runtime is named by its id and says nothing else; the
    # base says nothing at all.
    if a.kind == "app":
        entries = desktop_entries(a.diffdir)
        info = metainfo(a.diffdir, a.id.split(".")[-1])
        # A COMPONENT THAT NAMES NOBODY'S PROGRAM IS NO COMPONENT. A pack with
        # no desktop entry of its own — gmic, ngspice — has only its layer's
        # libraries to choose from, and the best of those by rank was
        # libgphoto2, so `app.gmic` described itself as a camera library.
        # Nothing is better than somebody else's sentence.
        if not entries and info.get("_rank", 3) >= 3:
            info = {}
    else:
        entries, info = [], {}

    # THE ENTRY THAT MATCHES THE PACK COMES FIRST, because entries[0] is what
    # names the row and supplies its icon. Debian ships xterm as
    # `debian-uxterm.desktop` and `debian-xterm.desktop`, and alphabetical
    # order made "UXTerm" the name of the pack called app.xterm.
    want = a.id.split(".")[-1]
    def rank(e):
        eid, ex = e["_id"], os.path.basename(e.get("Exec", "").split(" ")[0])
        if eid == want or ex == want:
            return 0
        if eid.endswith("-" + want) or ex.startswith(want):
            return 1
        if want in eid:
            return 2
        return 3
    entries.sort(key=lambda e: (rank(e), e["_id"]))

    lines = [
        "id          = %s" % a.id,
        "kind        = %s" % a.kind,
        "version     = %s" % a.version,
        "release     = 1",
        "arch        = x86_64",
    ]
    name = entries[0].get("Name") if entries else a.id
    lines.append("name        = %s" % name)
    if info.get("summary"):
        lines.append("summary     = %s" % info["summary"])
    elif entries and entries[0].get("Comment"):
        lines.append("summary     = %s" % entries[0]["Comment"])
    for p in info.get("description", [])[:6]:
        lines.append("description = %s" % p)
    if info.get("licence"):
        lines.append("licence     = %s" % info["licence"])
    if entries:
        cats = [c for c in entries[0].get("Categories", "").split(";") if c]
        # The FIRST non-generic category: `GTK` and `Qt` are toolkit tags and
        # naming a page after one would file GIMP under its widget set.
        for c in cats:
            if c not in ("GTK", "Qt", "KDE", "GNOME", "Application"):
                lines.append("category    = %s" % c)
                break
    if a.parent and a.parent != "-":
        lines.append("requires    = %s" % a.parent)
    # A DATA PACK IS NOT COMPOSED INTO A BOX ROOT — it is mounted noexec and
    # grafted — so an app that needs one names it here rather than requiring
    # it. `needs` is the honest word: without it the program opens with an
    # empty database, which is a broken menu entry rather than software.
    for n in a.needs:
        lines.append("needs       = %s" % n)
    for g in a.graft:
        lines.append("graft       = %s" % g)
    for g in a.boxgraft:
        lines.append("boxgraft    = %s" % g)

    mimes = []
    for e in entries:
        lines.append("desktop     = %s.desktop" % e["_id"])
        for m in e.get("MimeType", "").split(";"):
            if m and m not in mimes:
                mimes.append(m)
        # The command is the Exec line's first word with its path dropped: a
        # shim is named for what somebody types, not for where it lives.
        ex = e.get("Exec", "").split()
        if ex:
            cmd = os.path.basename(ex[0].strip('"'))
            if cmd and cmd not in ("env", "sh", "bash") and \
               ("command     = %s" % cmd) not in lines:
                lines.append("command     = %s" % cmd)
    # SOME ALIEN SOFTWARE IS A COMMAND, NOT AN APPLICATION. wine is the case:
    # what you want is `wine setup.exe` at a prompt, and its Debian entries are
    # NoDisplay, which the parse above correctly drops — so the pack would carry
    # wine and the host would have no way to reach it. A row names the command
    # instead; a launcher for `wine` with no arguments opens nothing.
    for c in a.command:
        if ("command     = %s" % c) not in lines:
            lines.append("command     = %s" % c)
    for m in mimes:
        lines.append("mime        = %s" % m)
    # A pack states the environment its own packages need. kdos-appbox exports
    # the whole stack's worth on entry, nearest pack winning, so a runtime can
    # select the platform theme it installed without a line of C anywhere.
    for e in a.env:
        lines.append("env         = %s" % e)
    if a.recommended:
        lines.append("recommended = yes")

    with open(a.out, "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines) + "\n")

    if a.icon and entries:
        src = find_icon(a.diffdir, entries[0].get("Icon", ""))
        if src:
            with open(src, "rb") as fh:
                data = fh.read()
            if png_is_blank(data):
                print("%s: icon %s decodes fully transparent — dropped"
                      % (a.id, src), file=sys.stderr)
            else:
                with open(a.icon, "wb") as fh:
                    fh.write(data)
    print(a.out)


if __name__ == "__main__":
    main()
