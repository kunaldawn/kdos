## About

libtorrent-rasterbar is Arvid Norberg's BitTorrent library in C++ on Boost.Asio, with its Python
binding. It is the torrent engine of [qbittorrent](#port-qbittorrent), which links the library,
and of [deluge](#port-deluge), whose daemon drives it through the binding.

## Notes

The port stays on the 2.0 series. Deluge 2.2.0 is written against the 2.0 Python API: its core
sets `ignore_resume_timestamps`, `cache_size`, `cache_expiry` and `half_open_limit`, names the
encryption levels `lt.enc_level.plaintext`, `rc4` and `both`, and reads `torrent_status.paused`.
A 2.0 build with deprecated functions on, the CMake default, is ABI version 1 and keeps every one
of those names. 2.1 builds ABI version 2, which drops them, and `deluged` stops at start with
`unknown name in settings_pack`. qBittorrent builds against either series; its
`libtorrent-2.1.patch` takes effect only from 2.1.0 on (`LIBTORRENT_VERSION_NUM >= 20100`). The
series moves when a Deluge release supports 2.1. The library's soname names its series
(`libtorrent-rasterbar.so.2.0`), so a change of series needs qBittorrent's `release` bumped in the
same change; a `qbittorrent` linked against the other series does not start.

`-Dpython-install-system-dir=ON` puts the binding in the system Python's `site-packages` rather
than under the CMake prefix, and `-Dpython-egg-info=ON` writes its egg-info beside it, so pip and
`importlib.metadata` see an installed `libtorrent`. `-Dgnutls=OFF` keeps OpenSSL: the GnuTLS build
compiles DHT out.
