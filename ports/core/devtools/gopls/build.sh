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

tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz -C $vendordir

# THE MODULE IS NOT AT THE TARBALL ROOT. golang.org/x/tools tags a gopls
# release and ships gopls as a SUBDIRECTORY with its own go.mod, so both the
# vendoring and the build happen one level down — `vendordir =` above is what
# tells ports/fetch that, and vendoring at the root would silently produce a
# bundle for the PARENT x/tools module, which builds something else.
cd $vendordir
export CGO_ENABLED=0
go build -mod=vendor -ldflags "-s -w" -o gopls
install -Dm755 gopls $PKG/usr/bin/gopls
