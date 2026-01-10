# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#    KDOS – forged by hand.
#    KD's Homebrew OS
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

COPY src /workspace/src
COPY fs /workspace/fs
COPY script /workspace/script
COPY ports /workspace/ports


RUN chmod 777 /workspace /workspace/fs
RUN chmod +x /workspace/script/*.sh

CMD ["/workspace/script/build.py"]
