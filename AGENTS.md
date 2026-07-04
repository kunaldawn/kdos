# KDOS — Agent Instructions

See `CLAUDE.md` for the full reference. This file is the quick-start.

## What this is

A hand-built Linux distro (musl + toybox, Wayland-only, no SystemD). The build system lives in this repo; the actual OS rootfs is produced inside Docker.

## Critical rules

- **No SystemD.** Use seatd, basu, eudev, dbus, dnsmasq, wpa_supplicant.
- **No Xorg.** Packages that pull X deps must be configured with X disabled.
- **No GTK on host.** Goes in distrobox via `kdos-fetch-app`.
- **No auto-commit.** User commits manually, often squashing. Don't commit without an explicit request.
- **No destructive git ops** without asking.
- **No rationale comments in kpkgbuilds.** Banner header + `# description` / `# homepage` / `# depends` only.
- **No sed/awk for source patching.** Use build flags first; sed only when unavoidable.

## Key commands

```sh
make fetch     # download all upstream tarballs
make build     # build everything inside Docker -> ISO
make run       # boot ISO in QEMU/KVM
make clean     # rm -rf build/
```

## Repo anatomy

| Path | Purpose |
|------|---------|
| `ports/core/<name>/kpkgbuild` | Package recipe (sourced bash, no shebang) |
| `script/00_toolchain/` .. `06_packaging/` | Build phases (scripts or `packages.txt`) |
| `script/build.py` | Python TUI orchestrator |
| `fs/` | Copied verbatim into rootfs at build time |
| `src/kpkg/` | kpkg package manager (bash) |
| `testing/test_runner.py` | Build individual ports in Docker for CI |

## kpkgbuild essentials

- No shebang — kpkg `source`s the file
- `$SRC` = extracted source dir, `$PKG` = DESTDIR staging tree, `$PORT_SRC` = dir containing kpkgbuild
- `# depends` parsing: `^# depends[[:blank:]]*:` space-separated list
- `vendoring=rust` in kpkgbuild enables `cargo vendor` — needs `tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz` in build()
- First `source=` tarball extracts with `--strip-components=1`, subsequent ones without

## Common build gotchas

| Issue | Fix |
|-------|-----|
| Rust + bindgen: "Dynamic loading not supported" | `export RUSTFLAGS="-C target-feature=-crt-static"` + `export LIBCLANG_PATH=/usr/lib` |
| CMake 4.x: "Compatibility with CMake < 3.5" | `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` |
| GCC 15: incompatible-pointer-types | `export CFLAGS="$CFLAGS -Wno-incompatible-pointer-types"` |
| libxkbcommon not found at runtime | Meson: `--prefix=/usr --libdir=lib` (not `/usr/local/lib64`) |
| libunwind missing assembly symbols | `export LDFLAGS="$LDFLAGS -Wl,--allow-shlib-undefined"` per consumer |
| ICU: ubrk_* symbols missing | `export LDFLAGS="$LDFLAGS -licuuc"` |
| `kpkg install -f` doesn't force rebuild | Workaround: `kpkgdel <pkg>` + `rm cache/*.tar.xz` then rebuild |

## Testing

```sh
# Prepare Docker base image (one time)
python3 testing/prepare_base.py
# Test a single port
python3 testing/test_runner.py <portname>
# Results: testing/test_results.json, logs in testing/logs/
```

## Working state checks

```sh
ls ports/core | wc -l              # port count (~360)
git status --short | wc -l          # tracked changes
ls build/fs/var/lib/kpkg/db/        # installed in chroot
tail -40 build/logs/04_phase4/*.log # debug build failures
```

## Git LFS

Tarballs (`*.tar.gz`, `*.tar.xz`) are tracked in Git LFS. `make fetch` downloads source URLs into the port directories.
