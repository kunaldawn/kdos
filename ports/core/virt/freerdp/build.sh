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

# The client is sdl-freerdp, on SDL3, which draws through Wayland. There is no
# X11 client (xfreerdp) and no Wayland one (wlfreerdp, deprecated in 3.x and
# built on its own uwac toolkit); CMAKE_DISABLE_FIND_PACKAGE_X11 keeps the
# SDL client's optional X11 window helpers from linking libX11 because it
# happens to be installed. The SDL2 client is off, or sdl2-compat would build
# a second copy of the same client.
#
# No server side: the shadow server captures an X11 display, which is not
# here, and nothing needs the proxy. Remmina links the client libraries.
#
# FFmpeg does H.264 (the GFX channel's AVC420/444) and scaling; OpenH264 is
# off, as are GSM, LAME, FAAD2 and FAAC, which only the experimental DSP
# formats use. Each codec is named, on or off: an OPTIONAL feature switched
# on fails configure when its library is missing, so the list here is the
# whole codec set and a missing library stops the build rather than a codec.
#
# The smartcard channel loads libpcsclite at run time rather than linking it,
# so pcsc-lite in `depends` is what makes redirecting a reader work. PKCS#11
# is the same: modules are loaded by path when a smartcard logon names one.
#
# The clipboard converts images to and from PNG, JPEG and WebP, so a picture
# copied on the remote side pastes into a Wayland application, which offers
# and asks for image/png rather than the BMP a Windows clipboard holds.
#
# JSON-C is named for the Azure AD logon and the timezone data; left to
# itself the build takes whichever of cJSON, json-c or jansson it finds first.
#
# uriparser parses the host names and URLs the client checks (redirection
# targets, gateway and proxy hosts, certificate names) as RFC 3986; without
# it the check is a short regex.
cmake -B build -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_TESTING=OFF \
	-DWITH_SAMPLE=OFF \
	-DWITH_SERVER=OFF \
	-DWITH_CLIENT=ON \
	-DWITH_CLIENT_SDL=ON \
	-DWITH_CLIENT_SDL2=OFF \
	-DWITH_CLIENT_SDL3=ON \
	-DWITH_SDL_IMAGE_DIALOGS=ON \
	-DWITH_X11=OFF \
	-DWITH_WAYLAND=OFF \
	-DCMAKE_DISABLE_FIND_PACKAGE_X11=ON \
	-DWITH_CHANNELS=ON \
	-DWITH_MANPAGES=ON \
	-DWITH_OPENSSL=ON \
	-DWITH_FFMPEG=ON \
	-DWITH_VIDEO_FFMPEG=ON \
	-DWITH_DSP_FFMPEG=OFF \
	-DWITH_SWSCALE=ON \
	-DWITH_CAIRO=OFF \
	-DWITH_JPEG=ON \
	-DWINPR_UTILS_IMAGE_JPEG=ON \
	-DWINPR_UTILS_IMAGE_PNG=ON \
	-DWINPR_UTILS_IMAGE_WEBP=ON \
	-DWITH_OPENH264=OFF \
	-DWITH_GSM=OFF \
	-DWITH_LAME=OFF \
	-DWITH_FAAD2=OFF \
	-DWITH_FAAC=OFF \
	-DWITH_AOM=OFF \
	-DWITH_DAV1D=OFF \
	-DWITH_YUV=OFF \
	-DWITH_FDK_AAC=ON \
	-DWITH_OPUS=ON \
	-DWITH_SOXR=ON \
	-DWITH_ALSA=ON \
	-DWITH_PULSE=ON \
	-DWITH_OSS=OFF \
	-DWITH_SNDIO=OFF \
	-DWITH_CUPS=ON \
	-DWITH_PCSC=ON \
	-DWITH_PKCS11=ON \
	-DWITH_KRB5=ON \
	-DWITH_FUSE=ON \
	-DWITH_JSONC_REQUIRED=ON \
	-DWITH_TIMEZONE_ICU=ON \
	-DWITH_URIPARSER=ON \
	-DWITH_SYSTEMD=OFF \
	-DWITH_INSTALL_CLIENT_DESKTOP_FILES=OFF \
	-DWITH_BINARY_VERSIONING=OFF \
	-DWITH_CCACHE=OFF \
	-DWITH_CLANG_FORMAT=OFF \
	-DUSE_VERSION_FROM_GIT_TAG=OFF
cmake --build build
DESTDIR=$PKG cmake --install build
