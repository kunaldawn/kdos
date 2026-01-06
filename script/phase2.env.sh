#!/bin/bash
# Environment configuration for KDOS build

export CHROOT=1

export CFLAGS="-O2 -pipe -std=gnu11 -fPIC"
export CXXFLAGS="-O2 -pipe -fPIC"
export LDFLAGS=""
export MAKEFLAGS="-j12"
export TERM=dumb

rm -rf /var/cache/kpkg/work

echo "Phase 2 ENV is active..."
