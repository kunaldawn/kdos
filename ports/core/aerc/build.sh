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

tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz
export CGO_ENABLED=1
# notmuch IS the reason this is here rather than mutt: aerc queries a notmuch
# database directly, so mail that is already indexed on the machine is
# searchable from the client with no second index. That binding is cgo, which
# is why CGO_ENABLED is on for this port and off for every other Go one.
make PREFIX=/usr GOFLAGS="-mod=vendor -tags=notmuch"
make PREFIX=/usr DESTDIR=$PKG install

# THE FILTER THAT SPOOLS, written HERE and not in a file beside the recipe
# because the recipe hash covers kpkgbuild, build.sh, postinstall.sh and
# *.patch and nothing else. A script of its own would be invisible to it: the
# port would report itself current after every later edit and the image would
# keep the renderer it already had. testing/preflight.sh extracts this heredoc
# and parses it, which is what a file would have got for free.
#
# It lands in aerc's own filter directory because aerc prepends every search
# directory's filters/ to the $PATH it runs a filter with, so /etc/skel's
# aerc.conf names it by bare word and carries no path an upstream
# --libexecdir change would break.
install -d "$PKG/usr/libexec/aerc/filters"
cat > "$PKG/usr/libexec/aerc/filters/kdos-part" <<'KDOS_SH'
#!/bin/sh
# kdos-part — one spool, five renderers, for aerc's [filters].
#
# ONE SPOOL, BECAUSE mutool OPENS A DOCUMENT BY PATH. aerc hands a part to a
# filter on STDIN and four of the five renderers read a pipe perfectly well;
# `mutool draw ... -` answers "cannot open -". One shape for all five is
# simpler than one exception, and the file costs nothing a message view does
# not already cost.
#
# NO ARM MAY `exec`. The trap is what removes the spool, and exec replaces this
# shell and its traps together; the file would then outlive every message
# viewed and hold its bytes in $TMPDIR for the rest of the login.
#
# NO SHELL EXPANSION OF THE ARGUMENT. The word below is matched against a fixed
# list and every command is written out in full: a filter line is
# configuration, and configuration that reached `eval` would be a shell for
# anyone who can write to a config file.
set -e

t=$(mktemp "${TMPDIR:-/tmp}/aerc-part.XXXXXX") || exit 1
trap 'rm -f "$t"' EXIT INT TERM HUP
cat > "$t"

case "$1" in
# -q BECAUSE A FILTER'S STDERR IS THE MESSAGE BODY. aerc gives every filter the
# pager's own pipe for stderr, so mutool's per-page progress line would be read
# as part of the document. Errors still print, which is what the reader needs.
pdf)
	mutool draw -q -F txt -o - "$t"
	;;
image)
	# --animate off OR A SENDER CHOOSES HOW LONG THE MESSAGE TAKES. chafa
	# animates a GIF even into a pipe, sleeping each frame's delay, and a
	# GIF's delay field is 16-bit centiseconds with no limit on the frame
	# count: a 219-byte attachment held the body for a minute here. Nothing
	# on aerc's side times a filter out.
	#
	# aerc puts the view's width in $COLUMNS; the fallback is for a hand
	# run. Height is fixed because the pager scrolls. `less -Rc` is that
	# pager and passes the colour through.
	chafa --format symbols --animate off \
		--size "${COLUMNS:-100}x30" "$t"
	;;
docx)
	docx2txt "$t" -
	;;
# THE ISOLATION IS PROBED, NOT ASSUMED. aerc's own html filter picks the
# namespace path on `command -v unshare`; the binary is on this image, so on a
# kernel without unprivileged user namespaces that filter runs a command which
# cannot start, and the reader gets `unshare: Operation not permitted` where
# the message should be. Probing means the worst case is a message shown
# without isolation rather than no message.
#
# ONE PROCESS EITHER WAY. The argument vector is built in the positional
# parameters and unshare prefixes it. Re-entering this script under unshare
# instead would read a stdin the first `cat` has already drained to end of
# file, and every HTML message would render empty.
html)
	# w3m LAYS TABLES OUT ON THE GRID, which is why it and not lynx is this
	# image's text browser and why marketing HTML — which is nothing but
	# tables — is readable at all. Always -dump: aerc pipes this into its
	# pager, so the filter must not take the terminal.
	set -- w3m -I UTF-8 -O UTF-8 -T text/html \
		-s -graph -cols "${COLUMNS:-100}" -dump \
		-o fold_line=true -o decode_url=true -o display_link=true \
		-o disable_center=true \
		-o no_cache=true -o use_cookie=false
	if unshare --map-root-user --net true >/dev/null 2>&1; then
		unshare --map-root-user --net "$@" "$t"
	else
		# Best effort without a namespace: an unroutable proxy, so a
		# tracking pixel resolves to a connection nothing can make.
		"$@" -o use_proxy=true \
			-o http_proxy=127.0.0.1:1 \
			-o https_proxy=127.0.0.1:1 "$t"
	fi
	;;
# A READING, NOT AN IMPORT. aerc's calendar filter writes nothing, which is the
# property that matters: a filter runs every time a message scrolls past, and
# one that imported an invitation would accept meetings by being looked at. It
# is run through gawk BY NAME rather than by its own `#!/usr/bin/awk -f`,
# because /usr/bin/awk on this image is toybox's. `command -v` finds the script
# because aerc puts its own filter directory on this filter's PATH.
ics)
	_cal=$(command -v calendar) || {
		echo "kdos-part: aerc's calendar filter is not on the path" >&2
		exit 2
	}
	gawk -f "$_cal" "$t"
	;;
*)
	echo "kdos-part: no renderer named '$1'" >&2
	exit 2
	;;
esac
KDOS_SH
chmod 755 "$PKG/usr/libexec/aerc/filters/kdos-part"

# %u IS THE ARGUMENT. kdos-appbox's opener SUBSTITUTES field codes and appends
# nothing when an entry has none (`exec_to_argv`, open.c), so a bare
# `Exec=aerc` opens an empty client for a link that named a person.
#
# NO `MimeType=x-scheme-handler/mailto;` HERE UNTIL AN OPENER HONOURS
# `Terminal=`. /usr/bin/xdg-open reads that key, and it runs the entry's Exec
# directly with no terminal: aerc then dies on `open /dev/tty: no such device`.
# A row that names a mail client belongs in /etc/xdg/mimeapps.list, beside an
# opener that knows this entry needs a terminal.
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/aerc.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Mail
GenericName=Email Client
Comment=Read and send mail
Exec=aerc %u
Icon=mail-message
Terminal=true
Categories=Network;Email;
Keywords=mail;email;imap;smtp;aerc;
EOF
chmod 644 "$PKG/usr/share/applications/aerc.desktop"
