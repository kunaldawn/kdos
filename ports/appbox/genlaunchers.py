#!/usr/bin/env python3
"""Generate everything the host needs to present the appbox's apps as its own.

usage: genlaunchers.py <desktop-dir-dump> <fs-root>

<desktop-dir-dump> is the image's /usr/share/applications. After an ISO build
the flattened appbox layer has it on disk already:
  build/fs/home/kdos/.local/share/containers/storage/overlay/*/diff/usr/share/applications

Parses each .desktop [Desktop Entry] section (first section only, so action
sections don't shadow Name/Icon like the old awk one-liner did), skips
NoDisplay/noise, applies a rename map so dock favorites keep their IDs, and
writes into <fs-root>:

  etc/skel/.local/share/applications/<upstream-id>.desktop
      the launcher, keeping UPSTREAM's own desktop-file id — not kdos-<name>,
      and not StartupWMClass either. Measured in a booted VM: cosmic-app-list
      matches a running toplevel to a desktop entry by the entry's FILE ID and
      ignores StartupWMClass, and a Wayland app_id is NOT the X11 WM_CLASS —
      GIMP's entry says StartupWMClass=gimp-3.0 but its toplevel announces
      app_id "gimp" (confirmed with WAYLAND_DEBUG=1), which is exactly its
      upstream filename. Get this wrong and every running alien app shows a
      second grey cog beside its own pinned icon. Carries the upstream
      MimeType, Keywords and GenericName —
      WITHOUT MimeType no alien app is offered in any "Open with" dialog and
      none can ever be a default handler, which is what "alien apps missing
      from the open dialog" was.
  etc/skel/.local/share/applications/mimeinfo.cache
      the mime -> desktop-id index. Written here rather than left to
      update-desktop-database: the host has no desktop-file-utils, and without
      the cache the association list above is never consulted.
  usr/share/kdos/alien-apps
      name -> in-box command line, read by /usr/local/bin/kdos-alien.
  usr/local/bin/<name> -> kdos-appbox
      one symlink per app, so every alien app is also a normal command in
      $PATH. kdos-appbox dispatches on its own basename, the way busybox does,
      which keeps the alien-app path free of any shell.
"""

import configparser
import os
import re
import sys

# Shim names that must never be created in /usr/local/bin. The host userland is
# musl + toybox + COSMIC and currently collides with none of the Debian app
# names, but the app set moves; a shim shadowing a host tool would be a very
# confusing bug.
RESERVED = {
    "sh", "bash", "env", "ls", "cp", "mv", "rm", "cat", "sed", "awk", "grep",
    "find", "tar", "gzip", "python3", "perl", "make", "gcc", "kdos", "foot",
    "kdos-appbox", "kdos-banner", "kdos-desktop", "kdos-shot",
}

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

# Extra argv an app needs to work inside the container / on this compositor,
# appended after the upstream Exec. All three VSCodium flags were established
# by launching it in a booted VM and reading why it died:
#   --no-sandbox                chrome-sandbox wants a setuid helper and
#                               CLONE_NEWUSER, gets neither as a non-root user
#                               in an unprivileged podman container, and exits
#                               rather than falling back
#   --ozone-platform-hint=auto  otherwise Electron does not pick Wayland
#   --disable-gpu-compositing   without it the renderer dies with
#                               "create_immed failed and produced an invalid
#                               wl_buffer" -> "launch-failed, code 1002".
#                               --disable-gpu also fixes it but turns off GPU
#                               rasterisation too; this is the smaller hammer.
EXEC_EXTRA = {
    "vscodium": "--no-sandbox --ozone-platform-hint=auto --disable-gpu-compositing",
}

# Environment assignments in an upstream Exec that force X11. KDOS is
# Wayland-only and cosmic-comp is not currently starting Xwayland, so these are
# a guaranteed silent failure: debian ships audacity as
# `env GDK_BACKEND=x11 audacity`, and with no X server GTK exits before
# printing anything at all. Measured: audacity runs fine on Wayland once the
# prefix is dropped.
X11_FORCING = {
    "GDK_BACKEND=x11", "CLUTTER_BACKEND=x11", "QT_QPA_PLATFORM=xcb",
    "SDL_VIDEODRIVER=x11", "MOZ_ENABLE_WAYLAND=0",
    "ELECTRON_OZONE_PLATFORM_HINT=x11",
}


def parse(srcdir):
    """Upstream entry -> the fields KDOS carries forward."""
    apps = []
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
        execline = de.get("Exec", "")
        cats = de.get("Categories", "")
        if not name or not execline:
            continue
        if "Settings" in cats and "System" not in cats:
            continue

        words = [w for w in execline.split()
                 if not (FIELD_CODE.fullmatch(w) and w not in KEEP_CODES)]
        if words and words[0] == "env":
            keep, i = ["env"], 1
            while i < len(words) and "=" in words[i]:
                if words[i] not in X11_FORCING:
                    keep.append(words[i])
                i += 1
            if len(keep) == 1:          # nothing left to set
                keep = []
            words = keep + words[i:]
        execline = " ".join(words)
        out = RENAME.get(base, base.lower())
        extra = EXEC_EXTRA.get(out)
        if extra:
            execline = (f"{execline} {extra}" if "%" not in execline
                        else execline.replace("%", f"{extra} %", 1))
        apps.append({
            "id": out,
            "base": base,
            "name": name,
            "exec": execline,
            "icon": de.get("Icon", ""),
            "cats": cats,
            "mime": de.get("MimeType", ""),
            "keywords": de.get("Keywords", ""),
            "generic": de.get("GenericName", ""),
            # The window this launcher opens announces the APP's app_id, not
            # ours, so without this the dock cannot tie a running alien app
            # back to any desktop entry and shows a generic placeholder.
            "wmclass": de.get("StartupWMClass") or base,
        })
    return apps


def write_launchers(apps, d):
    os.makedirs(d, exist_ok=True)
    # Clear by MARKER, not by name: these files have been called kdos-<id>
    # and <app_id> at different times and an orphan launcher is a dead icon.
    for fn in os.listdir(d):
        if not fn.endswith(".desktop"):
            continue
        p = os.path.join(d, fn)
        with open(p, encoding="utf-8", errors="replace") as f:
            if "X-KDOS-Alien=true" in f.read():
                os.unlink(p)
    for a in apps:
        lines = ["[Desktop Entry]", "Type=Application",
                 f"Name={a['name']}",
                 f"Comment={a['name']} (alien app, kdos-apps box)",
                 f"Exec=kdos-appbox run {a['exec']}",
                 f"Icon={a['icon']}", "Terminal=false",
                 f"Categories={a['cats']}"]
        if a["generic"]:
            lines.append(f"GenericName={a['generic']}")
        if a["mime"]:
            lines.append(f"MimeType={a['mime']}")
        if a["keywords"]:
            lines.append(f"Keywords={a['keywords']}")
        lines += [f"StartupWMClass={a['wmclass']}", "X-KDOS-Alien=true", ""]
        with open(os.path.join(d, f"{a['base']}.desktop"), "w") as f:
            f.write("\n".join(lines))


def write_mimeinfo(apps, d):
    index = {}
    for a in apps:
        for m in a["mime"].split(";"):
            m = m.strip()
            if m:
                index.setdefault(m, []).append(f"{a['base']}.desktop")
    with open(os.path.join(d, "mimeinfo.cache"), "w") as f:
        f.write("[MIME Cache]\n")
        for m in sorted(index):
            f.write("%s=%s;\n" % (m, ";".join(index[m])))
    return len(index)


def write_shims(apps, fsroot):
    table = os.path.join(fsroot, "usr/share/kdos/alien-apps")
    os.makedirs(os.path.dirname(table), exist_ok=True)
    with open(table, "w") as f:
        f.write("# name\tcommand — GENERATED by ports/appbox/genlaunchers.py\n")
        for a in apps:
            f.write("%s\t%s\n" % (a["id"], a["exec"]))

    bindir = os.path.join(fsroot, "usr/local/bin")
    # Clear every shim, whatever it used to point at — the dispatcher has
    # changed name once already and a stale symlink is a dead command.
    for fn in os.listdir(bindir):
        p = os.path.join(bindir, fn)
        if os.path.islink(p) and not os.readlink(p).startswith("/"):
            os.unlink(p)
    n = 0
    for a in apps:
        if a["id"] in RESERVED:
            print(f"shim {a['id']}: reserved name, skipped", file=sys.stderr)
            continue
        os.symlink("kdos-appbox", os.path.join(bindir, a["id"]))
        n += 1
    return n


def main():
    srcdir, fsroot = sys.argv[1], sys.argv[2]
    apps = parse(srcdir)
    appdir = os.path.join(fsroot, "etc/skel/.local/share/applications")
    write_launchers(apps, appdir)
    mimes = write_mimeinfo(apps, appdir)
    shims = write_shims(apps, fsroot)
    print(f"{len(apps)} launchers, {mimes} mime types, {shims} shims",
          file=sys.stderr)


if __name__ == "__main__":
    main()
