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

COPY src /workspace/src
COPY fs /workspace/fs
COPY script /workspace/script

RUN chmod 777 /workspace /workspace/fs
RUN chmod +x /workspace/script/build.sh

CMD ["/workspace/script/build.sh"]
