## About

libuv is the cross-platform asynchronous I/O library written for Node.js: an event loop over epoll,
with timers, TCP and UDP sockets, pipes, child processes, signal handling, and file system
operations and `getaddrinfo` lookups run on a thread pool.

In the tree it is linked by [cmake](#port-cmake), [nodejs](#port-nodejs) (`--shared-libuv`, in place
of the copy in Node's own source tree), [neovim](#port-neovim), [libwebsockets](#port-libwebsockets)
and [fcitx5](#port-fcitx5). It is built in `30_foundation`, the phase cmake is built in;
cmake depends on it, so every CMake recipe of a later phase needs it installed.

The release tarball from `dist.libuv.org` carries `configure.ac` and `Makefile.am` but no generated
`configure`, so `build.sh` first runs upstream's `autogen.sh`, which runs `libtoolize`, `aclocal`,
`autoconf` and `automake`. That is why the recipe depends on [autoconf](#port-autoconf),
[automake](#port-automake) and [libtool](#port-libtool).

## Flags

- `-g0` — added to the exported `CFLAGS`. libuv's `configure.ac` appends `-g` to its own
  `AM_CFLAGS`, which automake puts before the user's `CFLAGS` on every compile line of the library;
  `-g0` comes later on the line and cancels it, so the library is compiled without debugging
  information. The tree's own `CFLAGS` carry no `-g`.
