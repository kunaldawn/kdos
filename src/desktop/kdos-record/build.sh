#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# source= is empty: this is ours. basu and nothing else — the pipeline is
# gst-launch-1.0's, driven by argv, so GStreamer is a runtime dependency and
# not a link one.
gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
	$(pkg-config --cflags basu) \
	-o kdos-record "$PORT_SRC/main.c" \
	$(pkg-config --libs basu) $LDFLAGS

install -Dm755 kdos-record "$PKG/usr/bin/kdos-record"
