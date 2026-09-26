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
#   mbsync CAN, for XOAUTH2 only. isync reaches it through cyrus-sasl and the
#          cyrus-sasl-xoauth2 plugin, which wraps whatever `PassCmd` prints as
#          the bearer token — see ports/core/isync/build.sh. OAUTHBEARER has
#          no plugin here, so it is the one mechanism out of mbsync's reach.
#
# So a provider that has withdrawn application passwords is mirrored by mbsync
# when it offers XOAUTH2; one that offers only OAUTHBEARER is read in aerc
# directly and sent to through msmtp.
#
# NO sd-notify. It is optional and it is systemd's readiness protocol, which
# nothing on this machine speaks.
tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz

export RUSTFLAGS="-C target-feature=-crt-static"
cargo build --release --frozen --offline

install -Dm755 target/release/pizauth $PKG/usr/bin/pizauth
install -Dm644 pizauth.1 -t $PKG/usr/share/man/man1
install -Dm644 pizauth.conf.5 -t $PKG/usr/share/man/man5
install -Dm644 share/bash/completion.bash $PKG/usr/share/bash-completion/completions/pizauth
install -Dm644 share/fish/pizauth.fish $PKG/usr/share/fish/vendor_completions.d/pizauth.fish
install -Dm644 share/zsh/_pizauth $PKG/usr/share/zsh/site-functions/_pizauth
