## About

GNU libunistring is a library of Unicode string functions for C: conversion between UTF-8, UTF-16
and UTF-32, character properties, case mapping, normalisation, word and line breaking, and
display width. In KDOS its main users sit on the network path. [libidn2](#port-libidn2), which
turns an internationalised domain name into the form DNS carries, and [libpsl](#port-libpsl),
which reads the public suffix list that decides which domains may share a cookie, both link it, and curl
reaches it through them; [gnutls](#port-gnutls) and [guile](#port-guile) link it directly, and
[gettext](#port-gettext) is built against it rather than its bundled copy.

The recipe is upstream's autotools build with no switches of its own.
