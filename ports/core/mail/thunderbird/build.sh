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


# THUNDERBIRD 153 ESR: mail, calendar, contacts, chat and feeds. Everything it
# does works with no network once an account is configured by hand; the
# network-only paths (update checks, telemetry, add-on updates, the start page,
# captive-portal and connectivity probes, safe-browsing list updates) are off,
# by build flag where one exists and by the default prefs and policies below
# where none does.
#
# THE TOOLKIT IS cairo-gtk3-x11-wayland: both GDK backends are compiled in and
# linked, not probed for, so a missing X11 library is a configure error rather
# than a quietly Wayland-only build. GDK tries Wayland first at run time, and
# the X11 backend is what runs under Xwayland and what carries GTK 3's AT-SPI
# bridge for a screen reader.
#
# THE COMPILER IS clang with lld. Gecko's Rust bindings are generated through
# libclang, and its C++ is only tested with clang; gcc is not a supported
# compiler for this tree. CC and CXX are set here, over the phase's gcc, and
# the phase's -std=gnu11 is dropped: mozbuild sets every C file's standard.
#
# RUST_TARGET names the Rust target outright (fix-rust-target.patch). Mozilla
# otherwise matches the C triple against `rustc --print target-list`, and
# x86_64-pc-linux-musl matches both x86_64-unknown-linux-musl and
# x86_64-unikraft-linux-musl, which configure refuses as ambiguous.
#
# THE REST OF THE PATCHES are Alpine's musl set: lfs64 (musl has no stat64),
# musl-no-linux-prctl (linux/prctl.h and sys/prctl.h both define
# prctl_mm_map), sandbox-sched_setscheduler (musl's pthread calls
# sched_setscheduler, which the media sandbox otherwise kills), and
# fix-libresolv-path (musl's resolver is in libc, so the SRV and MX lookups of
# account setup find no libresolv). glean-stub and rust-lto-thin keep the
# Rust link inside a builder's memory: telemetry is off, so the stubbed
# metrics record nothing either way.
#
# NO WASM SANDBOX: RLBox needs wasi-sdk, which is not a port, so the libraries
# it would isolate (graphite, ogg, expat, hunspell, woff2) run in the content
# process as they do in every non-RLBox build.
#
# SYSTEM LIBRARIES where the tree's copy satisfies Mozilla's check. png stays
# in-tree because this libpng has no APNG, and av1 because aom is not a port.
# OpenPGP is the in-tree librnp on the in-tree Botan, with the system json-c,
# bzip2 and zlib.
#
# jemalloc is off: mozjemalloc is unproven on musl, so the client runs on
# musl's allocator.
#
# MOZILLA_OFFICIAL is left unset, and with it MOZ_TELEMETRY_REPORTING, so no
# telemetry upload code is compiled in.
patch -p1 -i "$PORT_SRC/fix-rust-target.patch"
patch -p1 -i "$PORT_SRC/glean-stub.patch"
patch -p1 -i "$PORT_SRC/lfs64.patch"
patch -p1 -i "$PORT_SRC/musl-no-linux-prctl.patch"
patch -p1 -i "$PORT_SRC/rust-lto-thin.patch"
patch -p1 -i "$PORT_SRC/sandbox-sched_setscheduler.patch"
patch -p1 -i "$PORT_SRC/fix-libresolv-path.patch"
patch -p1 -i "$PORT_SRC/metainfo.patch"

export CC=clang
export CXX=clang++
export CFLAGS="${CFLAGS/-std=gnu11/}"
export AR=llvm-ar
export NM=llvm-nm
export RANLIB=llvm-ranlib
export RUST_TARGET=x86_64-unknown-linux-musl
unset RUSTFLAGS MOZILLA_OFFICIAL MOZ_TELEMETRY_REPORTING

# mach builds its own virtualenv from the vendored packages under
# third_party/python. `none` installs nothing into it, so no pip reaches the
# network; `system` would fail on the first native package the host lacks.
export MACH_BUILD_PYTHON_NATIVE_PACKAGE_SOURCE=none
export MOZBUILD_STATE_PATH="$SRC_ROOT/mozbuild"
export MOZ_NOSPAM=1
export SHELL=/bin/sh

# The build ID is a timestamp baked into the binary and into every profile's
# compatibility.ini; taken from SOURCE_DATE_EPOCH, two builds of this recipe
# agree.
MOZ_BUILD_DATE=$(date -u -d "@$SOURCE_DATE_EPOCH" +%Y%m%d%H%M%S)
export MOZ_BUILD_DATE

cat > mozconfig <<CONF
ac_add_options --enable-application=comm/mail
ac_add_options --prefix=/usr
ac_add_options --enable-default-toolkit=cairo-gtk3-x11-wayland
ac_add_options --enable-official-branding
ac_add_options --enable-update-channel=release
ac_add_options --with-distribution-id=org.kdos
ac_add_options --with-unsigned-addon-scopes=app,system

ac_add_options --disable-bootstrap
ac_add_options --disable-crashreporter
ac_add_options --disable-updater
ac_add_options --disable-tests
ac_add_options --disable-debug
ac_add_options --disable-debug-symbols
ac_add_options --disable-strip
ac_add_options --disable-install-strip
ac_add_options --disable-jemalloc
ac_add_options --disable-necko-wifi
ac_add_options --disable-cargo-incremental
ac_add_options --without-wasm-sandboxed-libraries

ac_add_options --enable-release
ac_add_options --enable-optimize
ac_add_options --enable-hardening
ac_add_options --enable-linker=lld
ac_add_options --with-libclang-path=/usr/lib
ac_add_options --enable-dbus
ac_add_options --enable-alsa
ac_add_options --enable-pulseaudio
ac_add_options --disable-jack
ac_add_options --disable-sndio

ac_add_options --with-system-nspr
ac_add_options --with-system-nss
ac_add_options --with-system-icu
ac_add_options --with-system-jpeg
ac_add_options --with-system-webp
ac_add_options --with-system-libvpx
ac_add_options --with-system-libevent
ac_add_options --with-system-ffi
ac_add_options --with-system-zlib
ac_add_options --with-system-pixman
ac_add_options --with-system-jsonc
ac_add_options --with-system-bz2
ac_add_options --with-system-pipewire
ac_add_options --with-system-gbm
ac_add_options --with-system-libdrm

mk_add_options MOZ_OBJDIR=$SRC/obj
CONF
export MOZCONFIG="$SRC/mozconfig"

# mach runs its own make with its own job count; the phase's MAKEFLAGS is
# passed on as mach's -j, and an inherited one would be read twice.
_jobs=${MAKEFLAGS#-j}
unset MAKEFLAGS
./mach build ${_jobs:+-j"$_jobs"}
DESTDIR="$PKG" ./mach install

_app=/usr/lib/thunderbird

# Default prefs for an offline machine. A file under defaults/pref is read at
# every start and a user's own prefs.js still overrides it, so each line is a
# default, not a lock.
install -Dm644 /dev/stdin "$PKG$_app/defaults/pref/kdos-prefs.js" <<'PREFS'
pref("intl.locale.requested", "");
pref("spellchecker.dictionary_path", "/usr/share/hunspell");
pref("mail.shell.checkDefaultMail", false);
pref("mailnews.start_page.enabled", false);
pref("app.update.auto", false);
pref("extensions.update.enabled", false);
pref("extensions.update.autoUpdateDefault", false);
pref("extensions.getAddons.cache.enabled", false);
pref("datareporting.healthreport.uploadEnabled", false);
pref("datareporting.policy.dataSubmissionEnabled", false);
pref("toolkit.telemetry.enabled", false);
pref("toolkit.telemetry.unified", false);
pref("toolkit.telemetry.archive.enabled", false);
pref("network.captive-portal-service.enabled", false);
pref("network.connectivity-service.enabled", false);
pref("browser.safebrowsing.malware.enabled", false);
pref("browser.safebrowsing.phishing.enabled", false);
pref("browser.safebrowsing.downloads.remote.enabled", false);
PREFS

# Policies are what a user cannot turn back on from the settings page:
# no update check, no telemetry, no add-on update ping.
install -Dm644 /dev/stdin "$PKG$_app/distribution/policies.json" <<'POLICIES'
{
  "policies": {
    "DisableAppUpdate": true,
    "DisableTelemetry": true,
    "ExtensionUpdate": false
  }
}
POLICIES

# Dictionaries and hyphenation come from hunspell-en and hyphen, shared with
# every other speller on the machine, rather than from a bundled copy.
ln -sfn /usr/share/hunspell "$PKG$_app/dictionaries"
ln -sfn /usr/share/hyphen "$PKG$_app/hyphenation"

_brand=comm/mail/branding/thunderbird
for s in 16 22 24 32 48 64 128 256; do
	install -Dm644 "$_brand/default$s.png" \
		"$PKG/usr/share/icons/hicolor/${s}x$s/apps/thunderbird.png"
done
install -Dm644 "$_brand/net.thunderbird.Thunderbird.appdata.xml" \
	"$PKG/usr/share/metainfo/net.thunderbird.Thunderbird.appdata.xml"

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/thunderbird.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Thunderbird
GenericName=Mail, Calendar and Contacts
Comment=Read and write mail, keep a calendar and address book, follow feeds
Exec=thunderbird %u
Icon=thunderbird
Terminal=false
StartupWMClass=thunderbird
Categories=Network;Email;Calendar;ContactManagement;Feed;
MimeType=message/rfc822;x-scheme-handler/mailto;text/calendar;text/vcard;text/x-vcard;
Keywords=mail;email;e-mail;calendar;contacts;address book;feeds;rss;news;imap;smtp;
DESKTOP
chmod 644 "$PKG/usr/share/applications/thunderbird.desktop"
