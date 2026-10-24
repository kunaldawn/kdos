## About

nghttp3 implements HTTP/3, the mapping of HTTP onto QUIC (RFC 9114), and QPACK, its header
compression (RFC 9204), as the C library `libnghttp3`. It does not carry a QUIC transport of its
own: the caller pairs it with one, and in this tree that is [ngtcp2](#port-ngtcp2).

[curl](#port-curl) links the two for its HTTP/3 support, and [wireshark](#port-wireshark) links
nghttp3 to decompress the headers of HTTP/3 traffic it captures.

## Flags

- `--enable-lib-only` — build the library only. The one thing it leaves out is the `examples`
  directory, whose `qpack` program is a `noinst_PROGRAMS` target compiled with the C++ compiler and
  never installed.
