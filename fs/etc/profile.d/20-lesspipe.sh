# What `less` shows for a file that is not text.
#
# LESSOPEN names a filter less runs first: lesspipe.sh turns an archive into
# its listing, a compressed file into its contents, a PDF into its text, an
# image into its dimensions. Without it `less` on a `.tar.gz` is the bytes.
#
# THE FILTER IS DRIVEN BY `file -L -s -b --mime` AND BY NOTHING ELSE. An
# implementation without a magic database answers nothing there, and lesspipe
# then hands every file through unchanged — which looks exactly like no filter
# at all. That is why `file` on this image is the one with the database.
#
# NOT SET IF THE PERSON ALREADY SET IT: a filter somebody chose outranks the
# distribution's, and the same rule the desktop keeps about handlers.
if [ -z "$LESSOPEN" ] && [ -x /usr/bin/lesspipe.sh ]; then
	export LESSOPEN="|/usr/bin/lesspipe.sh %s"
fi

# -R passes the colour a filter emits through instead of showing the escapes.
[ -n "$LESS" ] || export LESS=-R
