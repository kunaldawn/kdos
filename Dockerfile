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

# Add deps for GCC build: gmp-dev mpfr-dev mpc1-dev
RUN apk update && apk add --no-cache \
    nano \
    git \
    curl \
    wget \
    bash bash-doc \
    coreutils coreutils-doc \
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
    python3

WORKDIR /workspace

CMD ["/workspace/script/build.py"]
