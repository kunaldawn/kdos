# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

FROM alpine:3.23

# GMP, MPFR and MPC are not installed: the cross GCC builds them in its own tree.
RUN apk update && apk add --no-cache \
    nano \
    git \
    curl \
    wget \
    bash bash-doc \
    coreutils coreutils-doc \
    grep sed gawk findutils diffutils patch \
    texinfo \
    make \
    gcc gcc-doc \
    g++ \
    musl-dev \
    bison \
    flex \
    linux-headers \
    rsync \
    xz \
    tar \
    zstd \
    python3

# Snapshot archives and the build TUI both assume UTF-8.
ENV LANG=C.UTF-8

WORKDIR /workspace

CMD ["/workspace/script/kdosbuild.sh"]
