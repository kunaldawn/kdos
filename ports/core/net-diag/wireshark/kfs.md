## About
[Wireshark]{.idx} is the network protocol analyser: it captures traffic and dissects it, protocol by
protocol, into fields. The package holds the Qt window `wireshark`, the command-line analyser
`tshark`, the capture program `dumpcap`, the file tools (`editcap`, `mergecap`, `capinfos`,
`text2pcap` and the rest), and the extcap helpers that capture from somewhere other than a local
interface. [termshark](#port-termshark) drives `tshark` and dissects nothing itself.

`dumpcap` is installed with no setuid bit and no file capability, so only root can capture; the
window started from the menu opens a saved capture but lists no interfaces (see
[setuid binaries](../../../../docs/kdos/03-architecture/security-model.md#setuid-binaries)).

## Flags
- `-DBUILD_wireshark=ON` — build the Qt window alongside the command-line tools. Upstream's
  default, named: the window and `tshark` are one package.
- `-DBUILD_androiddump=OFF` — leave out `androiddump`, the extcap that captures an Android
  device's logcat and interfaces through `adb`. Upstream's default is on.
- `-DBUILD_sshdump=ON` — with `-DBUILD_ciscodump=ON` and `-DBUILD_wifidump=ON`: the extcaps that
  capture on a remote host over SSH, through [libssh](#port-libssh): with `tcpdump` there, from a
  Cisco router's capture buffer, and Wi-Fi frames in monitor mode. Upstream's defaults, named.
- `-DBUILD_sdjournal=OFF` — leave out `sdjournal`, the extcap that reads the systemd journal.
  KDOS has no systemd (see
  [No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd)); upstream's default
  on Linux is on, and builds the extcap whenever it finds libsystemd.
- `-DBUILD_mmdbresolve=ON` — `mmdbresolve`, the helper that looks addresses up in a MaxMind
  GeoIP database through [libmaxminddb](#port-libmaxminddb). Upstream's default, named.
- `-DENABLE_ZLIBNG=OFF` — link [zlib](#port-zlib) only, not zlib-ng's native API. Upstream looks
  for zlib-ng as well as zlib and uses it wherever it is found, and [zlib-ng](#port-zlib-ng) is
  installed in an earlier phase without being in `depends`, so left on, the package would link a
  library it does not declare.
- `-DENABLE_MINIZIPNG=OFF` — the same for ZIP support: upstream tries minizip-ng first and falls
  back to minizip only when it is not found. Off, `-DENABLE_MINIZIP=ON` links the
  [minizip](#port-minizip) port, which is in `depends`; [minizip-ng](#port-minizip-ng) is
  installed in an earlier phase without being in it.
- `-DENABLE_CPUINFO=OFF` — no CPU detection through the cpuinfo library, which is not a port.
- `-DENABLE_ILBC=OFF` — with `-DENABLE_SPANDSP=OFF`, `-DENABLE_BCG729=OFF`, `-DENABLE_AMRNB=OFF`
  and `-DENABLE_AMRWB=OFF`: no iLBC, G.722/G.726, G.729, AMR-NB or AMR-WB decoding in the RTP
  player. Their libraries, libilbc, spandsp, bcg729 and opencore-amr, are not ports (see
  [Features whose library has no port](../../../../docs/kdos/06-reference/known-gaps.md#features-whose-library-has-no-port)).
  `-DENABLE_SBC=ON` and `-DENABLE_OPUS=ON` keep the Bluetooth SBC and Opus decoders, through
  [sbc](#port-sbc) and [opus](#port-opus).
- `-DENABLE_PCAP=ON` — and the remaining `ENABLE_` switches set on (`LUA`, `KERBEROS`, `GNUTLS`,
  `PKCS11`, `NGHTTP2`, `NGHTTP3`, `BROTLI`, `ZLIB`, `ZSTD`, `LZ4`, `XXHASH`, `NETLINK`, `CAP`,
  `SMI`, `MINIZIP`, `SNAPPY`, `PLUGINS`): capture through libpcap, Lua dissectors, Kerberos and TLS
  decryption, PKCS#11 keys, HTTP/2 and HTTP/3 header decompression, the compressed payload and file
  formats, libnl for Wi-Fi monitor mode, POSIX capabilities in `dumpcap`, SNMP MIBs through
  libsmi, and the bundled plugins. Each is upstream's default; upstream turns one off without an
  error when its library is missing, which is why `build.sh` checks the result (see Notes).
- `-DENABLE_WERROR=OFF` — do not turn compiler warnings into errors. Upstream's default is on, and
  a newer compiler's new warnings would then fail the build.

## Notes
Every optional library in Wireshark's configuration is found or skipped quietly. After `cmake`,
`build.sh` greps `config.h` for the `HAVE_` macro of each optional library the port depends on, from
`HAVE_LIBPCAP` to `HAVE_SNAPPY`, and greps `CMakeCache.txt` for `Qt6Multimedia_DIR`, which the RTP
player's audio output needs; a library that was not found stops the build with its name, rather
than shipping a package without the feature.

`ninja docs` must run before `ninja install`: with [asciidoctor](#port-asciidoctor) present, the
install rule names every manual page in roff and HTML and stops on the first one missing. The HTML
pages ship, because the window's Help > Manual Pages opens them (see
[Manual pages](../../../../docs/kdos/05-developer/writing-ports.md#manual-pages)).

After the install, `build.sh` removes the Stratoshark release notes, and every manual page, roff
and HTML, whose program is not in the package; upstream installs the pages of programs this build
leaves out, such as `androiddump`.

`build.sh` writes its own `org.wireshark.Wireshark.desktop` over upstream's. It keeps upstream's
`MimeType` list of capture formats and adds `StartupWMClass` and search keywords.
