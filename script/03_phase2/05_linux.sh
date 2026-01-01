#!/bin/bash
set -e
source script/phase2.env.sh


kpkg install -f flex bison pkgconf autoconf automake libtool texinfo bc elfutils gzip rsync linux
