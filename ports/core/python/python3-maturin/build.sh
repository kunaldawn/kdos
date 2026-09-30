# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# MATURIN BOOTSTRAPS ITSELF WITH setuptools-rust, which is what its own sdist's
# `backend-path = ["maturin"]` / `build-backend = "bootstrap"` pair is for: the
# published wheels are prebuilt binaries and this is the from-source path.
tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz
export CARGO_HOME="$SRC_ROOT/.cargo"
export RUSTFLAGS="-C target-feature=-crt-static"
export CARGO_NET_OFFLINE=true

# setup.py builds with --no-default-features unless MATURIN_SETUP_ARGS is set,
# and the variable replaces that argument list whole. This is upstream's default
# `full` set less `xwin`, the Windows MSVC cross-link: completions, scaffolding
# (`maturin new`), upload over rustls, SBOM output, auditwheel repair, and
# `--zig`, which runs the zig port at run time.
export MATURIN_SETUP_ARGS="--no-default-features --features cli-completion,scaffolding,upload,rustls,sbom,auditwheel,zig"

# --no-build-isolation: setuptools and setuptools-rust are installed ports, and
# an isolated environment would fetch them over a network this build does not
# have.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
