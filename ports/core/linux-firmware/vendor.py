#!/usr/bin/env python3
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
#   vendor.py linux-firmware-<date>.tar.gz
#
# HOST-ONLY, run by hand with network, exactly like kdos-icons/vendor.py and
# kdos-cursors/vendor.py. It prunes upstream into ONE committed tarball;
# nothing on the target or in the build container runs it.
#
# Why vendored rather than shipping upstream's tarball: upstream is 1.9 GB
# extracted / 927 MB compressed, and 90% of it is hardware KDOS will never
# run on — Snapdragon laptops, 100-gigabit NICs, DVB tuners, 1990s SCSI. The
# art packages already established the pattern (Papirus 90 MB -> 13.7 MB,
# Bibata 27 MB -> 4.4 MB); this is the same trade at a larger scale.
#
# Blobs are stored .zst because the kernel is built with
# CONFIG_FW_LOADER_COMPRESS_ZSTD=y and loads them compressed. That is a ~2.5:1
# saving on the target for no userspace work at all.
#
# ── The symlink trap, and why it does not bite here ────────────────────
#
# A pruned linux-firmware is famous for stranding symlinks: drop `mrvl` and
# `libertas/sd8688.bin` points at nothing. A dangling firmware symlink is not
# a visible error — request_firmware() just fails and the device silently does
# not work.
#
# Measured: the git tree contains ZERO symlinks. Upstream creates them at
# `make install` time from the `Link:` directives in WHENCE, so a file-copy
# vendoring never makes one to strand. The sweep below is kept anyway, costs
# nothing, and reports its count — if upstream ever checks links in, this
# notices instead of shipping dead paths.
#
# The flip side of not using upstream's installer: no dedup pass either, so
# duplicate blobs are stored twice. At 261 MB that is not worth solving.

import os, shutil, subprocess, sys, tarfile

# Verified against the tree, not assumed. Three of these are not where the
# obvious guess puts them: Intel Wi-Fi is intel/iwlwifi (not top-level
# iwlwifi), Intel Bluetooth is intel/ibt-* (loose files, not a directory),
# and there is no Intel SOF audio firmware in this repo at all — see GAPS.
KEEP_DIRS = [
    "amdgpu", "radeon", "i915", "xe",            # GPU — boot-blocking, see below
    "intel/iwlwifi",                             # Intel Wi-Fi
    "ath10k", "ath11k", "ath12k", "ath9k_htc", "ar3k",   # Qualcomm/Atheros
    "mediatek",                                  # mt76 — ubiquitous in 2024+ laptops
    "rtw88", "rtw89", "rtlwifi", "rtl_bt", "rtl_nic",    # Realtek
    "brcm", "cypress",                           # Broadcom
    "cirrus",                                    # cs35l41/43/56 speaker amps
    "amd-ucode", "amd",                          # AMD microcode + misc
]
KEEP_GLOBS = ["intel/ibt-*"]                     # Intel Bluetooth

# Kept deliberately small and loud rather than silently absent.
GAPS = """
  intel SOF audio   NOT in linux-firmware — upstream ships it separately as
                    thesofproject/sof-bin. Tiger Lake and newer have no audio
                    without it. Needs its own port.
  regulatory.db     NOT in linux-firmware — it is the wireless-regdb project.
                    kdos.config sets CONFIG_CFG80211_REQUIRE_SIGNED_REGDB=y,
                    so without it Wi-Fi is stuck in world-roaming: no 5 GHz
                    DFS channels and reduced TX power. Needs its own port.
  nvidia            dropped on purpose (154 MB). nouveau is built as a module
                    but there is no NVIDIA userspace on KDOS.
  qcom              dropped on purpose (502 MB) — Snapdragon laptops only.
"""

def main():
    if len(sys.argv) < 2:
        sys.exit(f"usage: {sys.argv[0]} linux-firmware-<date>.tar.gz [outdir]")
    src = sys.argv[1]
    outdir = sys.argv[2] if len(sys.argv) > 2 else os.path.dirname(os.path.abspath(__file__))

    base = os.path.basename(src)
    version = base.replace("linux-firmware-", "").split(".tar")[0]
    work = os.path.join(outdir, ".vendor-work")
    tree = os.path.join(work, "tree")
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(tree)

    print(f"==> extracting {base}")
    root = None
    with tarfile.open(src) as t:
        root = t.getnames()[0].split("/")[0]
        t.extractall(work)
    up = os.path.join(work, root)

    print("==> selecting")
    import glob as _g
    picked = []
    for d in KEEP_DIRS:
        s = os.path.join(up, d)
        if not os.path.exists(s):
            print(f"  !! {d}: absent upstream, skipped")
            continue
        picked.append(d)
        shutil.copytree(s, os.path.join(tree, d), symlinks=True)
    for pat in KEEP_GLOBS:
        for s in _g.glob(os.path.join(up, pat)):
            rel = os.path.relpath(s, up)
            os.makedirs(os.path.dirname(os.path.join(tree, rel)), exist_ok=True)
            shutil.copy2(s, os.path.join(tree, rel), follow_symlinks=False)

    # WHENCE is upstream's provenance and licence map. It is the only text
    # file worth keeping: it says who owns each blob and under what terms.
    for meta in ("WHENCE", "LICENCE.*", "LICENSE.*"):
        for s in _g.glob(os.path.join(up, meta)):
            shutil.copy2(s, os.path.join(tree, os.path.basename(s)))

    print("==> dropping stranded symlinks")
    dropped = 0
    for _ in range(4):          # links can point at links
        again = 0
        for dirpath, _dirs, files in os.walk(tree):
            for f in files:
                p = os.path.join(dirpath, f)
                if os.path.islink(p) and not os.path.exists(p):
                    os.unlink(p); dropped += 1; again += 1
        if not again:
            break

    print("==> compressing blobs (zstd, kernel loads them as-is)")
    nz = 0
    for dirpath, _dirs, files in os.walk(tree):
        for f in files:
            p = os.path.join(dirpath, f)
            if os.path.islink(p) or f.endswith((".zst", ".xz")) or f in ("WHENCE",):
                continue
            if f.startswith("LICEN"):
                continue
            subprocess.run(["zstd", "-q", "-19", "--rm", "-T0", p], check=True)
            nz += 1

    out = os.path.join(outdir, f"linux-firmware-{version}.tar.zst")
    print(f"==> packing {out}")
    subprocess.run(
        ["tar", "--sort=name", "--owner=0", "--group=0", "--numeric-owner",
         "--mtime=@0", "-C", tree, "-cf", out, "--zstd", "."], check=True)

    size = os.path.getsize(out) / (1 << 20)
    raw = subprocess.run(["du", "-sm", tree], capture_output=True, text=True).stdout.split()[0]
    print(f"\nkept {len(picked)} dirs  ·  {nz} blobs compressed  ·  "
          f"{dropped} stranded symlinks dropped")
    print(f"tree {raw} MB  ->  tarball {size:.0f} MB")
    print("\nKNOWN GAPS (deliberate, not oversights):" + GAPS)
    shutil.rmtree(work, ignore_errors=True)

main()
