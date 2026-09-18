# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# The compiled MIME database is generated, not shipped: `globs`, `types` and
# `mime.cache` are built from /usr/share/mime/packages by a program that has to
# run on the TARGET, which is why build.sh cannot do it. Without this the
# z-machine type the package just installed is an XML file nothing reads, and
# `Enter` on a story file falls through to whatever claims text.
#
# The same hook, and the same reason, as shared-mime-info's own.
update-mime-database /usr/share/mime
