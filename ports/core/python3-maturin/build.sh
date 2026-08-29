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

# --no-build-isolation: setuptools and setuptools-rust are installed ports, and
# an isolated environment would fetch them over a network this build does not
# have.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
