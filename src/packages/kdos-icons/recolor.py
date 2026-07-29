#!/usr/bin/env python3
"""Generate the KDOS icon theme: phosphor-recolored copies of the Cosmic
scalable places/categories/devices/mimetypes icons plus the two panel button
icons from hicolor. Symbolic icons are skipped (the toolkit tints them from
the active COSMIC theme already). Everything else is inherited from Cosmic.

usage: recolor.py <cosmic-scalable-dir> <hicolor-apps-dir> <out-theme-dir> [hue] [minsat]
"""

import colorsys
import os
import re
import sys

HEX_RE = re.compile(r"#([0-9a-fA-F]{6}|[0-9a-fA-F]{3})\b")

HUE = float(sys.argv[4]) if len(sys.argv) > 4 else 0.30      # phosphor green
MINSAT = float(sys.argv[5]) if len(sys.argv) > 5 else 0.80


def shift(match):
    raw = match.group(1)
    if len(raw) == 3:
        raw = "".join(c * 2 for c in raw)
    r, g, b = (int(raw[i : i + 2], 16) / 255.0 for i in (0, 2, 4))
    l, s = colorsys.rgb_to_hls(r, g, b)[1:]
    if l < 0.02 or l > 0.98:
        return match.group(0)
    r, g, b = colorsys.hls_to_rgb(HUE, l, max(s, MINSAT))
    return "#%02x%02x%02x" % (round(r * 255), round(g * 255), round(b * 255))


def recolor_file(src, dst):
    with open(src, "r", encoding="utf-8") as f:
        data = f.read()
    with open(dst, "w", encoding="utf-8") as f:
        f.write(HEX_RE.sub(shift, data))


def main():
    cosmic, hicolor_apps, out = sys.argv[1], sys.argv[2], sys.argv[3]
    contexts = {
        "places": "Places",
        "categories": "Categories",
        "devices": "Devices",
        "mimetypes": "MimeTypes",
    }

    dirs = []
    for ctx in contexts:
        srcdir = os.path.join(cosmic, ctx)
        if not os.path.isdir(srcdir):
            continue
        dstdir = os.path.join(out, "scalable", ctx)
        os.makedirs(dstdir, exist_ok=True)
        dirs.append(f"scalable/{ctx}")
        for name in sorted(os.listdir(srcdir)):
            if not name.endswith(".svg") or name.endswith("-symbolic.svg"):
                continue
            recolor_file(os.path.join(srcdir, name), os.path.join(dstdir, name))

    appsdir = os.path.join(out, "scalable", "apps")
    os.makedirs(appsdir, exist_ok=True)
    dirs.append("scalable/apps")
    for name in (
        "com.system76.CosmicPanelLauncherButton.svg",
        "com.system76.CosmicPanelAppButton.svg",
    ):
        src = os.path.join(hicolor_apps, name)
        if os.path.isfile(src):
            recolor_file(src, os.path.join(appsdir, name))

    dirs.append("256x256/apps")
    lines = [
        "[Icon Theme]",
        "Name=KDOS",
        "Comment=KDOS phosphor icon theme",
        "Inherits=Cosmic,Pop,hicolor",
        "Directories=" + ",".join(dirs),
        "",
    ]
    for d in dirs:
        if d.startswith("scalable/"):
            ctx = contexts.get(d.split("/", 1)[1], "Applications")
            lines += [f"[{d}]", f"Context={ctx}", "Size=64", "MinSize=8",
                      "MaxSize=512", "Type=Scalable", ""]
        else:
            lines += [f"[{d}]", "Context=Applications", "Size=256", "Type=Fixed", ""]
    with open(os.path.join(out, "index.theme"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines))


if __name__ == "__main__":
    main()
