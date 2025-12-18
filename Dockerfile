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
    man-pages \
    gdb \
    ctags \
    ncurses-dev \
    flex \
    bison \
    linux-headers \
    diffutils \
    elfutils-dev \
    cpio cpio-doc \
    findutils \
    openssl-dev \
    syslinux syslinux-doc \
    pkgconf \
    rsync \
    gmp-dev \
    mpfr-dev \
    mpc1-dev \
    mpfr-dev \
    mpc1-dev \
    perl \
    xz \
    autoconf \
    automake \
    libtool \
    python3 \
    ncurses \
    xorriso \
    mtools \
    dosfstools \
    util-linux-dev \
    squashfs-tools \
    go

WORKDIR /workspace

COPY src /workspace/src
COPY fs /workspace/fs
COPY script /workspace/script

RUN chmod 777 /workspace /workspace/fs
RUN chmod +x /workspace/script/*.sh

CMD ["/workspace/script/build.sh"]
