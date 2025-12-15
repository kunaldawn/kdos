FROM alpine:3.23

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
    cdrkit cdrkit-doc

WORKDIR /workspace

COPY src/linux-6.18.1.tar.xz /workspace/src/linux-6.18.1.tar.xz
COPY src/toybox-0.8.13.tar.gz /workspace/src/toybox-0.8.13.tar.gz
COPY src/musl-1.2.5.tar.gz /workspace/src/musl-1.2.5.tar.gz
COPY src/config/.config.linux /workspace/src/config/.config.linux
COPY src/config/.config.toybox /workspace/src/config/.config.toybox

COPY fs/etc/init.d/rcS /workspace/fs/etc/init.d/rcS

COPY script/build.sh /workspace/script/build.sh

RUN chmod 777 /workspace /workspace/fs
RUN chmod +x /workspace/script/build.sh

CMD ["/workspace/script/build.sh"]
