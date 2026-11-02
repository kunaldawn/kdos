#!/usr/bin/env python3
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   lnfcheck — the KDOS global themes and desks hold only what Plasma reads
# ---------------------------------
#
#     lnfcheck.py [--root DIR]               the source tree
#     lnfcheck.py [--root DIR] --tree FS     the packages installed in FS
#     lnfcheck.py --self-test                prove the checks fail
#
# Run from the repository root (or give --root). Exit status 1 on any failure.
#
# A GLOBAL THEME FAILS QUIETLY. KLookAndFeelManager reads a fixed set of
# `[file][group] key` entries from a package's contents/defaults and ignores
# every other line, and a package with a file missing borrows Breeze's: a
# misspelt group, or a top-level [KSplash] (inert, as Kubuntu's and Garuda's
# are), applies nothing and says nothing, and a missing splash/ shows Plasma's
# splash. So each defaults file is held to the entries the manager reads, as
# plasma-workspace 6.8's libklookandfeel spells them, and:
#
#   - every font in [kdeglobals][General] needs a font key in
#     [kdeglobals][WM]: the manager APPLIES the General keys but DECIDES that a
#     package has fonts from WM, so without one no font is ever applied;
#   - every package has splash/Splash.qml, layouts/defaults and
#     layouts/org.kde.plasma.desktop-layout.js, and with --tree
#     previews/preview.png and previews/fullscreenpreview.jpg;
#   - ksplashrc names the package's own id.
#
# In the source tree the defaults are generated the way the build generates
# them, by src/desktop/kdos-plasma-look/genlook.py, for each accent read from
# libkcolor's KCOL_SCHEMES; with --tree the installed files are read.
#
# THE DESKS PIN WHAT THE FAVOURITES LIST NAMES. Every `applications:<id>.desktop`
# launcher in a desk template (src/desktop/kdos-plasma-look/desks) must be the
# first word of a line of fs/etc/skel/.config/kdos/favorites, the list of boxes
# warmed at login, or a fresh account has a pinned app whose box is cold. With
# --tree, every favourite must also be a desktop entry the image installs.
# A desk template must be in the `panel` containment category (loadTemplate()
# finds templates only through it) and NoDisplay (or Add Panel lists it).
# ---------------------------------

import argparse
import importlib.util
import json
import os
import re
import sys

# KLookAndFeelManager::packageContents() and save(), plasma-workspace 6.8.2
# libklookandfeel/klookandfeelmanager.cpp: the entries it reads, per file.
DEFAULTS_KEYS = {
    "kdeglobals][KDE": {"widgetStyle"},
    "kdeglobals][General": {"ColorScheme", "font", "fixed", "smallestReadableFont",
                            "toolBarFont", "menuFont", "activeFont"},
    "kdeglobals][WM": {"font", "fixed", "smallestReadableFont", "toolBarFont",
                       "menuFont", "activeFont"},
    "kdeglobals][Icons": {"Theme"},
    "plasmarc][Theme": {"name"},
    "Wallpaper": {"Image"},
    "kcminputrc][Mouse": {"cursorTheme"},
    "kwinrc][WindowSwitcher": {"LayoutName"},
    "kwinrc][org.kde.kdecoration2": {"library", "theme", "NoPlugin", "BorderSize"},
    "kwinrc][Windows": {"Placement"},
    "ksplashrc][KSplash": {"Theme", "Engine"},
}
LAYOUT_DEFAULTS_KEYS = {
    "kwinrc][org.kde.kdecoration2": {"ButtonsOnLeft", "ButtonsOnRight"},
    "kwinrc][Windows": {"BorderlessMaximizedWindows"},
}
GENERAL_FONTS = {"font", "fixed", "smallestReadableFont", "toolBarFont", "menuFont"}
WM_FONT_MARKERS = {"font", "fixed", "smallestReadableFont", "toolBarFont", "menuFont"}


class Report:
    def __init__(self):
        self.failures = []

    def fail(self, where, msg):
        self.failures.append("%s: %s" % (where, msg))


def parse_ini(text):
    groups = {}
    cur = None
    for n, raw in enumerate(text.split("\n"), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("[") and line.endswith("]"):
            cur = groups.setdefault(line[1:-1], {})
            continue
        if cur is None:
            groups.setdefault("", {})[line] = n
            continue
        key = line.split("=", 1)[0].strip()
        cur[key] = line.split("=", 1)[1].strip() if "=" in line else ""
    return groups


def check_defaults(where, text, own_id, rep):
    g = parse_ini(text)
    if "" in g:
        rep.fail(where, "lines before any group")
    for group, keys in g.items():
        if group == "":
            continue
        known = DEFAULTS_KEYS.get(group)
        if known is None:
            rep.fail(where, "[%s] is not a group KLookAndFeelManager reads%s" % (
                group, " (a top-level [KSplash] is inert: it is [ksplashrc][KSplash])" if group == "KSplash" else ""))
            continue
        for key in keys:
            if key not in known:
                rep.fail(where, "[%s] %s is not a key KLookAndFeelManager reads" % (group, key))
    general = g.get("kdeglobals][General", {})
    wm = g.get("kdeglobals][WM", {})
    if GENERAL_FONTS & set(general) and not WM_FONT_MARKERS & set(wm):
        rep.fail(where, "fonts in [kdeglobals][General] with no font key in [kdeglobals][WM]: they are never applied")
    splash = g.get("ksplashrc][KSplash", {}).get("Theme")
    if own_id and splash is not None and splash != own_id:
        rep.fail(where, "[ksplashrc][KSplash] Theme is %s, not the package's own %s" % (splash, own_id))
    if not general.get("ColorScheme"):
        rep.fail(where, "no [kdeglobals][General] ColorScheme")


def check_layout_defaults(where, text, rep):
    for group, keys in parse_ini(text).items():
        known = LAYOUT_DEFAULTS_KEYS.get(group)
        if known is None:
            rep.fail(where, "[%s] is not a group the layout defaults are read for" % group)
            continue
        for key in keys:
            if key not in known:
                rep.fail(where, "[%s] %s is not read from layouts/defaults" % (group, key))


def check_package(pkg_dir, rep, need_previews):
    own_id = os.path.basename(pkg_dir.rstrip("/"))
    where = os.path.relpath(pkg_dir)
    meta = os.path.join(pkg_dir, "metadata.json")
    try:
        with open(meta, encoding="utf-8") as f:
            data = json.load(f)
        if data.get("KPackageStructure") != "Plasma/LookAndFeel":
            rep.fail(where, "metadata.json KPackageStructure is not Plasma/LookAndFeel")
        if data.get("KPlugin", {}).get("Id") != own_id:
            rep.fail(where, "metadata.json KPlugin Id is not %s" % own_id)
    except (OSError, ValueError) as e:
        rep.fail(where, "metadata.json: %s" % e)
    needs = ["contents/defaults", "contents/splash/Splash.qml", "contents/layouts/defaults",
             "contents/layouts/org.kde.plasma.desktop-layout.js"]
    if need_previews:
        needs += ["contents/previews/preview.png", "contents/previews/fullscreenpreview.jpg"]
    for n in needs:
        if not os.path.isfile(os.path.join(pkg_dir, n)):
            rep.fail(where, "%s is missing; the Breeze package would stand in for it" % n)
    path = os.path.join(pkg_dir, "contents/defaults")
    if os.path.isfile(path):
        check_defaults(where + "/contents/defaults", open(path, encoding="utf-8").read(), own_id, rep)
    path = os.path.join(pkg_dir, "contents/layouts/defaults")
    if os.path.isfile(path):
        check_layout_defaults(where + "/contents/layouts/defaults", open(path, encoding="utf-8").read(), rep)


def favorites(root):
    path = os.path.join(root, "fs/etc/skel/.config/kdos/favorites")
    ids = []
    for raw in open(path, encoding="utf-8"):
        line = raw.strip()
        if line and not line.startswith("#"):
            ids.append(line.split()[0])
    return path, ids


def check_desks(root, rep, tree):
    desks = os.path.join(root, "src/desktop/kdos-plasma-look/desks")
    fav_path, favs = favorites(root)
    for name in sorted(os.listdir(desks)):
        d = os.path.join(desks, name)
        where = os.path.relpath(d, root)
        try:
            data = json.load(open(os.path.join(d, "metadata.json"), encoding="utf-8"))
        except (OSError, ValueError) as e:
            rep.fail(where, "metadata.json: %s" % e)
            continue
        if "panel" not in data.get("X-Plasma-ContainmentCategories", []):
            rep.fail(where, "not in the panel containment category: loadTemplate() cannot find it")
        if data.get("NoDisplay") is not True:
            rep.fail(where, "NoDisplay is not set: Add Panel would list the desk")
        if data.get("KPlugin", {}).get("Id") != name:
            rep.fail(where, "KPlugin Id is not %s" % name)
        js = os.path.join(d, "contents/layout.js")
        if not os.path.isfile(js):
            rep.fail(where, "contents/layout.js is missing")
            continue
        for app in re.findall(r'"applications:([^"]+)\.desktop"', open(js, encoding="utf-8").read()):
            if app not in favs:
                rep.fail(where + "/contents/layout.js",
                         "pins %s, which %s does not name" % (app, os.path.relpath(fav_path, root)))
    if tree:
        apps = os.path.join(tree, "usr/share/applications")
        for app in favs:
            if not os.path.isfile(os.path.join(apps, app + ".desktop")):
                rep.fail(os.path.relpath(fav_path, root), "%s is not a desktop entry in %s" % (app, apps))


def accents_from_kcolor(root):
    """(name, theme_name, deep) per row of libkcolor's KCOL_SCHEMES."""
    text = open(os.path.join(root, "src/libs/libkcolor/kcolor.h"), encoding="utf-8").read()
    body = text[text.index("#define KCOL_SCHEMES(X)"):]
    body = body[:body.index("\n\n")]
    rows = re.findall(r'X\(\s*(\w+)\s*,\s*"[^"]*"\s*,\s*"([^"]+)"\s*,\s*\\?\s*'
                      r'((?:[0-9a-fA-F]{6}\s*,\s*\\?\s*){8}[0-9a-fA-F]{6})\s*\)', body)
    out = []
    for name, theme, hexes in rows:
        values = re.findall(r"[0-9a-fA-F]{6}", hexes)
        out.append((name, theme, values[4]))
    return out


def load_genlook(root):
    # A __pycache__ left in the port directory is part of its recipe hash,
    # so importing the generator must write no bytecode beside it.
    sys.dont_write_bytecode = True
    path = os.path.join(root, "src/desktop/kdos-plasma-look/genlook.py")
    spec = importlib.util.spec_from_file_location("genlook", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


class SourceScheme:
    """What genlook's lnf_defaults reads of a scheme, from the libkcolor row."""

    def __init__(self, name, theme_name, deep):
        self.accent = name
        self.title = theme_name[len("KDOS-"):] if theme_name.startswith("KDOS-") else name.title()
        self.scheme = "KDOS" + self.title
        r, g, b = (int(deep[i:i + 2], 16) for i in (0, 2, 4))
        self.light = (r * 11 + g * 16 + b * 5) // 32 >= 192


def check_source(root, rep):
    gen = load_genlook(root)
    rows = accents_from_kcolor(root)
    if not rows:
        rep.fail("src/libs/libkcolor/kcolor.h", "no KCOL_SCHEMES rows parsed")
    template = os.path.join(root, "src/desktop/kdos-plasma-look/lookandfeel")
    for need in ("contents/splash/Splash.qml", "contents/layouts/defaults",
                 "contents/layouts/org.kde.plasma.desktop-layout.js",
                 "contents/plasmoidsetupscripts/org.kde.plasma.kickoff.js"):
        if not os.path.isfile(os.path.join(template, need)):
            rep.fail(os.path.relpath(template, root), "%s is missing from the template every package copies" % need)
    check_layout_defaults("lookandfeel/contents/layouts/defaults",
                          open(os.path.join(template, "contents/layouts/defaults"), encoding="utf-8").read(), rep)
    for name, theme, deep in rows:
        sc = SourceScheme(name, theme, deep)
        pkg_id = "org.kdos.%s.desktop" % name
        check_defaults("genlook %s contents/defaults" % pkg_id, gen.lnf_defaults(sc, pkg_id), pkg_id, rep)
        meta = json.loads(gen.lnf_metadata(sc, pkg_id))
        if meta.get("KPlugin", {}).get("Id") != pkg_id or meta.get("KPackageStructure") != "Plasma/LookAndFeel":
            rep.fail("genlook %s metadata.json" % pkg_id, "wrong Id or KPackageStructure")


def self_test():
    """Each check must fail on the case it exists for."""
    cases = {
        "a Kubuntu-style top-level [KSplash]":
            "[kdeglobals][General]\nColorScheme=X\n\n[KSplash]\nTheme=org.kdos.bone.desktop\n",
        "fonts with no [kdeglobals][WM] font key":
            "[kdeglobals][General]\nColorScheme=X\nfont=Noto Sans,10\n",
        "an unknown key":
            "[kdeglobals][General]\nColorScheme=X\nColourScheme=X\n",
        "a splash that is not the package's own":
            "[kdeglobals][General]\nColorScheme=X\n\n[ksplashrc][KSplash]\nTheme=org.kde.breeze.desktop\n",
    }
    ok = True
    for what, text in cases.items():
        rep = Report()
        check_defaults("self-test", text, "org.kdos.bone.desktop", rep)
        if not rep.failures:
            print("lnfcheck: self-test: %s was not caught" % what)
            ok = False
    rep = Report()
    check_defaults("self-test", "[kdeglobals][General]\nColorScheme=X\nfont=Noto Sans,10\n\n"
                   "[kdeglobals][WM]\nfont=Noto Sans,10\n", "org.kdos.bone.desktop", rep)
    if rep.failures:
        print("lnfcheck: self-test: a valid defaults file failed: %s" % rep.failures)
        ok = False
    print("lnfcheck: self-test %s" % ("ok" if ok else "FAILED"))
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser(description="the KDOS global themes and desks hold only what Plasma reads")
    ap.add_argument("--root", default=".")
    ap.add_argument("--tree", default=None, help="an installed tree (build/fs) to check instead of the source")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()
    if args.self_test:
        return self_test()
    root = os.path.abspath(args.root)
    rep = Report()
    if args.tree:
        lnf = os.path.join(args.tree, "usr/share/plasma/look-and-feel")
        pkgs = sorted(p for p in os.listdir(lnf) if p.startswith("org.kdos.")) if os.path.isdir(lnf) else []
        if not pkgs:
            rep.fail(lnf, "no org.kdos.* global theme is installed")
        for p in pkgs:
            check_package(os.path.join(lnf, p), rep, need_previews=True)
    else:
        check_source(root, rep)
    check_desks(root, rep, args.tree)
    for f in rep.failures:
        print("LNF  " + f)
    print("lnfcheck: %s" % ("ok" if not rep.failures else "%d problem(s)" % len(rep.failures)))
    return 1 if rep.failures else 0


if __name__ == "__main__":
    sys.exit(main())
