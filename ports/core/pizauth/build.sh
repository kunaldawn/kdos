# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# WHAT A TOKEN IS FOR, AND WHAT ON THIS IMAGE CAN PRESENT ONE. pizauth mints an
# OAuth2 access token and refreshes it; a mail program then has to know the
# mechanism that carries it.
#
#   aerc   CAN. Its own Go implementation: `imaps+oauthbearer://` and
#          `smtps+oauthbearer://`, with `auth.xoauth2Client` beside it.
#   msmtp  CAN. Built-in and not gsasl's — `msmtp --version` on this image
#          reports `Authentication library: built-in` and lists `oauthbearer`
#          and `xoauth2`, and smtp_auth_oauthbearer() sits outside the
#          `#endif /* !HAVE_LIBGSASL */`.
#   mbsync CANNOT, and no token changes that. isync reaches XOAUTH2 only
#          through cyrus-sasl, which this tree does not build, so the shipped
#          binary carries no such string at all — see ports/core/isync/build.sh.
#
# So a provider that has withdrawn application passwords is reachable here by
# reading mail in aerc directly and sending through msmtp; what it cannot be is
# MIRRORED into a local Maildir by mbsync.
#
# NO sd-notify. It is optional and it is systemd's readiness protocol, which
# nothing on this machine speaks.
tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz

cargo build --release --frozen --offline

install -Dm755 target/release/pizauth $PKG/usr/bin/pizauth
