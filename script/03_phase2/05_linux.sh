#!/bin/bash
set -e
source script/phase2.env.sh


kpkg install -f pkgconf autoconf automake libtool texinfo bc flex bison elfutils gzip rsync linux
