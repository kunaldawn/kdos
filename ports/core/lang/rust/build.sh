#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

patch -p1 -i $PORT_SRC/rust-llvm23-amx-tf32.patch
patch -p1 -i $PORT_SRC/rust-llvm23-assign-guid.patch

mkdir -p build/cache/$_date
cp $PORT_SRC/rust-std-$_rust-$_triplet.tar.xz build/cache/$_date/
cp $PORT_SRC/rustc-$_rust-$_triplet.tar.xz build/cache/$_date/
cp $PORT_SRC/cargo-$_cargo-$_triplet.tar.xz build/cache/$_date/

cat << EOF > config.toml
[llvm]
targets = "X86"
link-shared = true

[build]
docs = false
extended = true
locked-deps = true
vendor = true
python = "/usr/bin/python3"
tools = ["cargo", "clippy", "rustdoc", "rustfmt", "src", "rust-analyzer-proc-macro-srv"]
description = "kdos"
profiler = true

[install]
prefix = "/usr"

[rust]
channel = "stable"
jemalloc = true
codegen-units-std = 1
rustflags = ["-Cstrip=debuginfo"]

[target.$_triplet]
llvm-config = "/usr/bin/llvm-config"
crt-static = false
EOF

mkdir "$SRC/rust"
export CARGO_HOME="$SRC/rust"
export RUST_BACKTRACE=1

export LIBSSH2_SYS_USE_PKG_CONFIG=1
export LIBGIT2_NO_VENDOR=1

python3 ./x.py build --jobs "$KDOS_JOBS"
DESTDIR=$PKG python3 ./x.py install -v --jobs "$KDOS_JOBS"

python3 ./x.py dist rustc-dev --jobs "$KDOS_JOBS"
mkdir "$SRC/rustc-dev"
tar -xf build/dist/rustc-dev-$version-$_triplet.tar.xz -C "$SRC/rustc-dev" --strip-components=1
"$SRC/rustc-dev/install.sh" --prefix=/usr --destdir="$PKG" --disable-ldconfig
for f in "$PKG"/usr/lib/rustlib/$_triplet/lib/*.so; do
	[ -e "$PKG/usr/lib/${f##*/}" ] || continue
	ln -sf "../../../${f##*/}" "$f"
done

rm -f "$PKG"/usr/lib/rustlib/{components,install.log,rust-installer-version,uninstall.sh} \
	"$PKG"/usr/lib/rustlib/manifest-*

install -d "$PKG/usr/share/bash-completion/completions"
mv "$PKG/etc/bash_completion.d/cargo" "$PKG/usr/share/bash-completion/completions/cargo"
rmdir "$PKG/etc/bash_completion.d"
