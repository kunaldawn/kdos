# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE RUST HALF IS A SECOND CARGO PROJECT AT src/rust, which is why the recipe
# carries `vendordir` — `cargo vendor` at the tarball ROOT finds no manifest.
#
# THE BUNDLE COMES BACK OUT AT THE ROOT, NOT AT src/rust. Cargo finds
# .cargo/config by walking up from the CURRENT DIRECTORY, and maturin invokes
# it from the project root with `--manifest-path src/rust/Cargo.toml` — so a
# config beside that manifest is never read and every crate in the bundle
# resolves as missing (`no matching package named 'base64'`, under --offline,
# with the crate sitting in vendor/ the whole time). Here the config's own
# relative `directory = "vendor"` resolves correctly as well.
tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz
export CARGO_HOME="$SRC_ROOT/.cargo"
export RUSTFLAGS="-C target-feature=-crt-static"
export CARGO_NET_OFFLINE=true

# --no-build-isolation: maturin and cffi are installed ports.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
