#!/usr/bin/env python3
"""Generate kdos-*.desktop launchers from the appbox image's desktop entries.

usage: genlaunchers.py <desktop-dir-dump> <out-dir>

Parses each .desktop [Desktop Entry] section (first section only, so action
sections don't shadow Name/Icon like the old awk one-liner did), skips
NoDisplay/noise, applies a rename map so dock favorites keep their IDs, and
writes uniform kdos-<name>.desktop wrappers that exec `kdos-appbox run ...`.
"""

import configparser
import os
import re
import sys

SKIP_BASENAMES = {
    "xfce4-about", "libfm-pref-apps", "lxshortcut", "pcmanfm-desktop-pref",
    "org.xfce.mousepad-settings", "libreoffice-xsltfilter",
    "org.gnome.gThumb.Import", "org.remmina.Remmina-file", "remmina-gnome",
    "assistant", "designer", "linguist",
    "bleachbit-root", "breezestyleconfig", "kcm_breezedecoration",
    "ktelnetservice6", "org.kde.kded6", "org.kde.kiod6", "org.kde.kwalletd6",
    "codium-url-handler", "gnome-disk-image-mounter", "gnome-disk-image-writer",
    "python3.13", "calibre-lrfviewer", "com.github.FontManager.FontViewer",
    "org.kicad.eeschema", "org.kicad.gerbview", "org.kicad.pcbnew",
}
SKIP_PREFIXES = ("krita_", "carla", "org.kicad.bitmap2component",
                 "org.kicad.pcbcalculator", "cups", "display-im", "mediainfo-gui")
# old-launcher names the dock favorites / docs already reference
RENAME = {
    "firefox-esr": "firefox",
    "org.xfce.mousepad": "mousepad",
    "org.gnome.SimpleScan": "simple-scan",
    "org.pwmt.zathura": "zathura",
    "transmission-gtk": "transmission",
    "org.musicbrainz.Picard": "picard",
    "com.github.xournalpp.xournalpp": "xournalpp",
    "com.github.maoschanz.drawing": "drawing",
    "com.github.johnfactotum.Foliate": "foliate",
    "com.obsproject.Studio": "obs",
    "org.gnome.gThumb": "gthumb",
    "org.gnome.Meld": "meld",
    "im.dino.Dino": "dino",
    "io.github.Hexchat": "hexchat",
    "org.remmina.Remmina": "remmina",
    "org.wireshark.Wireshark": "wireshark",
    "org.inkscape.Inkscape": "inkscape",
    "org.kde.krita": "krita",
    "org.darktable.darktable": "darktable",
    "org.freecad.FreeCAD": "freecad",
    "org.keepassxc.KeePassXC": "keepassxc",
    "org.kicad.kicad": "kicad",
    "org.octave.Octave": "octave",
    "org.shotcut.Shotcut": "shotcut",
    "org.stellarium.Stellarium": "stellarium",
    "org.musescore.MuseScore": "musescore",
    "org.zim_wiki.Zim": "zim",
    "fr.handbrake.ghb": "handbrake",
    "org.gnome.baobab": "baobab",
    "org.gnome.DiskUtility": "disks",
    "org.gnome.DejaDup": "dejadup",
    "libreoffice-startcenter": "libreoffice",
    "sol": "aisleriot",
    "supertux2": "supertux",
    "net.minetest.minetest": "luanti",
    "com.libretro.RetroArch": "retroarch",
    "io.mgba.mGBA": "mgba",
    "org.wesnoth.Wesnoth-1.18": "wesnoth",
    "io.github.wxmaxima_developers.wxMaxima": "wxmaxima",
    "io.github.xiaoyifang.goldendict_ng": "goldendict",
    "com.github.wwmm.easyeffects": "easyeffects",
    "org.hydrogenmusic.Hydrogen": "hydrogen",
    "com.github.jeromerobert.pdfarranger": "pdfarranger",
    "com.github.FontManager.FontManager": "font-manager",
    "org.gnome.GHex": "ghex",
    "org.zealdocs.zeal": "zeal",
    "org.bleachbit.BleachBit": "bleachbit",
    "org.fontforge.FontForge": "fontforge",
    "org.kde.kdenlive": "kdenlive",
    "org.scummvm.scummvm": "scummvm",
    "org.gnome.Chess": "gnome-chess",
    "org.gnome.Mines": "gnome-mines",
    "org.gnome.Sudoku": "gnome-sudoku",
    "org.gnome.Quadrapassel": "quadrapassel",
    "calibre-gui": "calibre",
    "calibre-ebook-edit": "calibre-editor",
    "calibre-ebook-viewer": "calibre-viewer",
    "PrusaSlicer": "prusa-slicer",
    "PrusaGcodeviewer": "prusa-gcodeviewer",
    "codium": "vscodium",
}
FIELD_CODE = re.compile(r"%[a-zA-Z]")
KEEP_CODES = {"%U", "%F", "%f", "%u"}

# Extra argv the app needs to work inside the container, appended after the
# upstream Exec. VSCodium is Electron: its chrome-sandbox wants a setuid
# helper and CLONE_NEWUSER, neither of which it gets as a non-root user inside
# an unprivileged podman container, and it exits instead of falling back.
EXEC_EXTRA = {
    "vscodium": "--no-sandbox",
}


def main():
    srcdir, outdir = sys.argv[1], sys.argv[2]
    os.makedirs(outdir, exist_ok=True)
    made = []
    for fn in sorted(os.listdir(srcdir)):
        if not fn.endswith(".desktop"):
            continue
        base = fn[:-8]
        if base in SKIP_BASENAMES or any(base.startswith(p) for p in SKIP_PREFIXES):
            continue
        cp = configparser.RawConfigParser(strict=False, interpolation=None)
        try:
            cp.read(os.path.join(srcdir, fn), encoding="utf-8")
        except Exception as e:
            print(f"skip {fn}: {e}", file=sys.stderr)
            continue
        if not cp.has_section("Desktop Entry"):
            continue
        de = cp["Desktop Entry"]
        if de.get("NoDisplay", "false").lower() == "true":
            continue
        if de.get("Type", "Application") != "Application":
            continue
        name = de.get("Name")
        icon = de.get("Icon", "")
        execline = de.get("Exec", "")
        cats = de.get("Categories", "")
        if not name or not execline:
            continue
        if "Settings" in cats and "System" not in cats:
            continue
        words = []
        for w in execline.split():
            if FIELD_CODE.fullmatch(w) and w not in KEEP_CODES:
                continue
            words.append(w)
        if words and words[0] == "env":
            pass  # keep env VAR=... prefixes intact
        execline = " ".join(words)
        out = RENAME.get(base, base.lower())
        extra = EXEC_EXTRA.get(out)
        if extra:
            execline = f"{execline} {extra}" if "%" not in execline else \
                execline.replace("%", f"{extra} %", 1)
        # The window this launcher opens comes from the container announcing
        # the APP's own app_id, not ours, so without this cosmic-app-list
        # cannot tie the toplevel back to any desktop entry and the dock shows
        # a generic placeholder for every running alien app. Upstream's own
        # StartupWMClass wins; otherwise the upstream desktop-file id is what
        # a GTK/Qt app sets by default.
        wmclass = de.get("StartupWMClass") or base
        with open(os.path.join(outdir, f"kdos-{out}.desktop"), "w") as f:
            f.write("[Desktop Entry]\n")
            f.write("Type=Application\n")
            f.write(f"Name={name}\n")
            f.write(f"Comment={name} (alien app, kdos-apps box)\n")
            f.write(f"Exec=kdos-appbox run {execline}\n")
            f.write(f"Icon={icon}\n")
            f.write("Terminal=false\n")
            f.write(f"Categories={cats}\n")
            f.write(f"StartupWMClass={wmclass}\n")
            f.write("X-KDOS-Alien=true\n")
        made.append(f"kdos-{out}.desktop")
    print("\n".join(made))
    print(f"total {len(made)}", file=sys.stderr)


if __name__ == "__main__":
    main()
