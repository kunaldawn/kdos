## About
Noto Sans CJK is Google and Adobe's sans-serif face for Chinese, Japanese and Korean, shipped as one
OpenType Collection, `NotoSansCJK.ttc`, that holds the Simplified Chinese, Traditional Chinese, Hong
Kong, Japanese and Korean variants and a monospaced variant of each. It is the system's one CJK
face, and so the fallback for any CJK character a program's own font lacks. Like the other Noto
ports, it installs the face upstream publishes rather than building it (see
[What is built from source](../../../../docs/kdos/05-developer/writing-ports.md#what-is-built-from-source)).

## Notes
The source is a `.zip`, which kpkg copies into `$SRC` whole rather than unpacking, so `build.sh`
unzips it itself; it holds only the collection and its licence.

The recipe writes `70-noto-cjk.conf` into `/usr/share/fontconfig/conf.avail` and enables it with a
symbolic link in `/etc/fonts/conf.d`. For text tagged with a CJK language (`ja`, `ko`, `zh-cn`,
`zh-sg`, `zh-tw`, `zh-hk`, `zh-mo`) it puts the variant for that language at the very front of
`sans-serif`, `serif` and `monospace` (`prepend_first`), so the same code point is drawn with
Japanese, Korean or the right Chinese glyph shapes. `serif` is mapped to the sans face, because no
CJK serif face is installed.

The collection is a large file, and [noto-fonts-extra](#port-noto-fonts-extra) deliberately does
not ship it a second time: two packages owning one path conflict at install time.
