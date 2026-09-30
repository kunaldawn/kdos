# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------


# THE NETWORK IS EMBEDDED IN THE BINARY, and the build fetches it unless the
# file evaluate.h names is already in src/ and its SHA-256 matches the name.
# It is a recipe source, so the build reaches no network; a different network
# is a different file name and fails that check.
cp "$_net.nnue" src/

# x86-64-universal compiles the engine once per instruction-set level, from
# plain x86-64 to AVX-512, and links them into one binary that picks the
# fastest the processor has at start-up. Any single level is either slow
# everywhere or an illegal instruction on an older machine. `build` is the
# target without profile-guided optimisation, whose profile comes from a timed
# benchmark run and so would differ from one build to the next.
cd src
make build ARCH=x86-64-universal COMP=gcc
install -Dm755 stockfish "$PKG/usr/bin/stockfish"
