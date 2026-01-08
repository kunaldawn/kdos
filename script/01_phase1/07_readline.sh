#!/bin/bash
set -e
source script/phase1.env.sh
source script/util/port.sh

if [ -f "$MARK/readline" ]; then
    exit 0
fi

echo ">>> Building readline..."

# Extract readline and dependencies from ports
READLINE_SRC=$(extract_port_source readline)

cd "$READLINE_SRC"
mkdir -p build && cd build

../configure \
    --host=$KDOS_TARGET \
    --prefix=/usr \
    --disable-static

make SHLIB_LIBS="-lncursesw"
make DESTDIR=$SYSROOT install

rm -rf "$READLINE_SRC"
touch "$MARK/readline"
