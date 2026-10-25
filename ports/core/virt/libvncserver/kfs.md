## About

LibVNCServer is the pair of C libraries for the VNC remote-framebuffer protocol: `libvncserver`
for writing a VNC server and `libvncclient` for writing a viewer. In KDOS
[Remmina](#port-remmina)'s VNC plugin links `libvncclient`. Only the libraries are built.

## Flags

- `-DLIBVNCSERVER_INSTALL=ON`, `-DBUILD_SHARED_LIBS=ON` — Generate the install target and build
  shared libraries. Both are upstream's defaults on Linux.
- `-DWITH_ZLIB=ON`, `-DWITH_LZO=ON`, `-DWITH_JPEG=ON`, `-DWITH_PNG=ON` — The compression libraries
  behind the zlib, ZRLE, Tight and TightPNG encodings; with LZO found, the bundled miniLZO is not
  compiled. Each of upstream's `WITH_` switches means "search for", and a library it does not find
  is left out without an error, so every one of them is named and the libraries are in `depends`.
- `-DWITH_THREADS=ON`, `-DWITH_TIGHTVNC_FILETRANSFER=ON` — Thread support, and the TightVNC file
  transfer extension, which upstream builds only with threads.
- `-DWITH_OPENSSL=ON` — OpenSSL as both the TLS layer and the cryptographic backend, used for
  VeNCrypt, Apple Remote Desktop and MSLogon authentication.
- `-DWITH_GNUTLS=OFF`, `-DWITH_GCRYPT=OFF` — The alternative TLS and cryptography libraries. Both
  are ports, and with them off the backend cannot change with what the build root holds.
- `-DWITH_SASL=ON` — SASL authentication through [cyrus-sasl](#port-cyrus-sasl), for servers that
  ask for it.
- `-DWITH_WEBSOCKETS=ON`, `-DWITH_IPv6=ON`, `-DWITH_24BPP=ON` — WebSocket transport, IPv6 and
  24-bit pixel formats. All three are upstream's defaults.
- `-DWITH_SYSTEMD=OFF` — No systemd socket activation, which would link libsystemd; KDOS has no
  systemd (see
  [No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd)).
- `-DWITH_SDL=OFF`, `-DWITH_GTK=OFF`, `-DWITH_QT=OFF`, `-DWITH_XCB=OFF`, `-DWITH_FFMPEG=OFF`,
  `-DWITH_LIBSSHTUNNEL=OFF` — The libraries that only upstream's example programs use: the SDL,
  GTK and Qt example viewers, the `x11` example server (XCB with its XTest and keysyms libraries),
  the VNC-to-MPEG encoder and the SSH-tunnelled client.
- `-DWITH_EXAMPLES=OFF`, `-DWITH_TESTS=OFF` — Leave out the example programs and the tests.
