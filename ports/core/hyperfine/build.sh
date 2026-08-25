#!/bin/bash


tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz

# `kdos march` ALREADY ENCODES THIS DISCIPLINE FOR ONE QUESTION: run it several
# times, take the median, measure the machine's own noise, and refuse a "win"
# that does not clear it. This is the same argument generalised to any command
# — and it is the tool to reach for before believing that a change made
# anything faster on a machine that is also compiling something.
cargo build --release --frozen --offline
install -Dm755 target/release/hyperfine $PKG/usr/bin/hyperfine
install -Dm644 doc/hyperfine.1 $PKG/usr/share/man/man1/hyperfine.1
