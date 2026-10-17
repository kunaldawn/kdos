#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

unzip -q -o $name-$version.zip

install -dm755 $PKG/usr/share/fonts/noto-cjk
install -m644 NotoSansCJK.ttc $PKG/usr/share/fonts/noto-cjk/
install -dm755 $PKG/usr/share/licenses/$name
install -m644 LICENSE $PKG/usr/share/licenses/$name/

install -dm755 $PKG/usr/share/fontconfig/conf.avail $PKG/etc/fonts/conf.d
cat > $PKG/usr/share/fontconfig/conf.avail/70-noto-cjk.conf <<'XML'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">
<fontconfig>
  <match target="pattern">
    <test name="lang" compare="contains"><string>ja</string></test>
    <test name="family"><string>sans-serif</string></test>
    <edit name="family" mode="prepend_first"><string>Noto Sans CJK JP</string></edit>
  </match>
  <match target="pattern">
    <test name="lang" compare="contains"><string>ja</string></test>
    <test name="family"><string>serif</string></test>
    <edit name="family" mode="prepend_first"><string>Noto Sans CJK JP</string></edit>
  </match>
  <match target="pattern">
    <test name="lang" compare="contains"><string>ja</string></test>
    <test name="family"><string>monospace</string></test>
    <edit name="family" mode="prepend_first"><string>Noto Sans Mono CJK JP</string></edit>
  </match>
  <match target="pattern">
    <test name="lang" compare="contains"><string>zh-tw</string></test>
    <test name="family"><string>sans-serif</string></test>
    <edit name="family" mode="prepend_first"><string>Noto Sans CJK TC</string></edit>
  </match>
  <match target="pattern">
    <test name="lang" compare="contains"><string>zh-tw</string></test>
    <test name="family"><string>serif</string></test>
    <edit name="family" mode="prepend_first"><string>Noto Sans CJK TC</string></edit>
  </match>
  <match target="pattern">
    <test name="lang" compare="contains"><string>zh-tw</string></test>
    <test name="family"><string>monospace</string></test>
    <edit name="family" mode="prepend_first"><string>Noto Sans Mono CJK TC</string></edit>
  </match>
  <match target="pattern">
    <test name="lang" compare="contains"><string>zh-hk</string></test>
    <test name="family"><string>sans-serif</string></test>
    <edit name="family" mode="prepend_first"><string>Noto Sans CJK HK</string></edit>
  </match>
  <match target="pattern">
    <test name="lang" compare="contains"><string>zh-hk</string></test>
    <test name="family"><string>serif</string></test>
    <edit name="family" mode="prepend_first"><string>Noto Sans CJK HK</string></edit>
  </match>
  <match target="pattern">
    <test name="lang" compare="contains"><string>zh-hk</string></test>
    <test name="family"><string>monospace</string></test>
    <edit name="family" mode="prepend_first"><string>Noto Sans Mono CJK HK</string></edit>
  </match>
  <match target="pattern">
    <test name="lang" compare="contains"><string>zh-mo</string></test>
    <test name="family"><string>sans-serif</string></test>
    <edit name="family" mode="prepend_first"><string>Noto Sans CJK HK</string></edit>
  </match>
  <match target="pattern">
    <test name="lang" compare="contains"><string>zh-mo</string></test>
    <test name="family"><string>serif</string></test>
    <edit name="family" mode="prepend_first"><string>Noto Sans CJK HK</string></edit>
  </match>
  <match target="pattern">
    <test name="lang" compare="contains"><string>zh-mo</string></test>
    <test name="family"><string>monospace</string></test>
    <edit name="family" mode="prepend_first"><string>Noto Sans Mono CJK HK</string></edit>
  </match>
  <match target="pattern">
    <test name="lang" compare="contains"><string>zh-cn</string></test>
    <test name="family"><string>sans-serif</string></test>
    <edit name="family" mode="prepend_first"><string>Noto Sans CJK SC</string></edit>
  </match>
  <match target="pattern">
    <test name="lang" compare="contains"><string>zh-cn</string></test>
    <test name="family"><string>serif</string></test>
    <edit name="family" mode="prepend_first"><string>Noto Sans CJK SC</string></edit>
  </match>
  <match target="pattern">
    <test name="lang" compare="contains"><string>zh-cn</string></test>
    <test name="family"><string>monospace</string></test>
    <edit name="family" mode="prepend_first"><string>Noto Sans Mono CJK SC</string></edit>
  </match>
  <match target="pattern">
    <test name="lang" compare="contains"><string>zh-sg</string></test>
    <test name="family"><string>sans-serif</string></test>
    <edit name="family" mode="prepend_first"><string>Noto Sans CJK SC</string></edit>
  </match>
  <match target="pattern">
    <test name="lang" compare="contains"><string>zh-sg</string></test>
    <test name="family"><string>serif</string></test>
    <edit name="family" mode="prepend_first"><string>Noto Sans CJK SC</string></edit>
  </match>
  <match target="pattern">
    <test name="lang" compare="contains"><string>zh-sg</string></test>
    <test name="family"><string>monospace</string></test>
    <edit name="family" mode="prepend_first"><string>Noto Sans Mono CJK SC</string></edit>
  </match>
  <match target="pattern">
    <test name="lang" compare="contains"><string>ko</string></test>
    <test name="family"><string>sans-serif</string></test>
    <edit name="family" mode="prepend_first"><string>Noto Sans CJK KR</string></edit>
  </match>
  <match target="pattern">
    <test name="lang" compare="contains"><string>ko</string></test>
    <test name="family"><string>serif</string></test>
    <edit name="family" mode="prepend_first"><string>Noto Sans CJK KR</string></edit>
  </match>
  <match target="pattern">
    <test name="lang" compare="contains"><string>ko</string></test>
    <test name="family"><string>monospace</string></test>
    <edit name="family" mode="prepend_first"><string>Noto Sans Mono CJK KR</string></edit>
  </match>
</fontconfig>
XML
ln -s ../../../usr/share/fontconfig/conf.avail/70-noto-cjk.conf \
	$PKG/etc/fonts/conf.d/70-noto-cjk.conf
