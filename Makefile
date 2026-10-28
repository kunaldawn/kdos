# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

all: build

# `make help` lists every target that carries a `## ` description on its rule
# line, in file order. A target without one does not appear.
help: ## list every target
	@awk -F':.*## ' '/^[a-zA-Z0-9_-]+:.*## /{printf "  %-18s %s\n", $$1, $$2}' $(MAKEFILE_LIST)

# Extra flags for the orchestrator, e.g.
#   make build BUILD_ARGS="--restore 20_selfhost"
#   make build BUILD_ARGS=--fresh
#   make build BUILD_ARGS=--snapshot
BUILD_ARGS ?=

# WHAT SIZE A VM COMES UP AT, for every run target and for testing/qemu-hw.
#
# 1920x1080 rather than QEMU's own 1280x800 default for virtio-gpu: the cell
# grid is the mode divided by the font's cell, so the default is a desktop of
# about 160x50 characters — enough to look like the screen is small and not
# enough to lay a three-column menu out the way it ships. It is the preferred
# mode the guest is told about through EDID, so the desktop picks it without
# being configured.
#
#   make run KDOS_RES=2560x1440
KDOS_RES ?= 1920x1080
KDOS_XRES = $(word 1,$(subst x, ,$(KDOS_RES)))
KDOS_YRES = $(word 2,$(subst x, ,$(KDOS_RES)))

# The virtual machine's disk. kinstall refuses a disk smaller than the install
# size plus 768 MiB, and every package is on the medium, so the install is
# about 53 GB; a qcow2 is sparse and takes on the host only what the guest
# writes.
#
#   make cleandisk KDOS_DISK=128G
KDOS_DISK ?= 96G

# Every port's sources from the archive, the cache or upstream, verified
# against the recipe. `make build` downloads no source, only the builder
# image's packages when that image is first built, so run this once after a
# clone and after any recipe change. fetch-check is offline and exits 1 naming
# each archived source missing or corrupt. FETCH_JOBS=N works on N ports at once.
FETCH_JOBS ?= 1

fetch: ## download every source a recipe names (networked)
	FETCH_JOBS=$(FETCH_JOBS) bash ports/fetch

fetch-check: ## offline: which archived sources are missing or corrupt
	FETCH_JOBS=$(FETCH_JOBS) bash ports/fetch --check

# The source archive and the system release, through ports/publish. The
# archive is the numbered pre-releases sources-1, sources-2, …, and the volume
# of each shelf is the second field of its line in ports/shelves; publish-plan
# gives a volume to every shelf without one, and publish-retire deletes the
# emptied src-<shelf> releases of the older one-release-per-shelf layout.
# Uploading needs the token in ~/.config/kdos/sources-token (mode 600) or
# KDOS_SOURCES_TOKEN; -dry, -check, -plan and -orphans need none. PORTS
# narrows publish, publish-dry and publish-rehome to those ports;
# PUBLISH_ARGS reaches any of them unchanged (--dry-run makes publish-plan,
# publish-rehome and publish-retire report only). TAG names the system tag
# for freeze and the release targets.
#   make publish-dry
#   make publish PORTS="zstd lz4"
#   make publish-retire PUBLISH_ARGS=--dry-run
#   make release TAG=v0.2
PORTS ?=
PUBLISH_ARGS ?=
TAG ?=

publish: ## upload every unarchived source into its shelf's sources-N volume
	bash ports/publish $(PUBLISH_ARGS) $(PORTS)

publish-dry: ## offline: what publish would upload, and where
	bash ports/publish --dry-run $(PUBLISH_ARGS) $(PORTS)

publish-check: ## which sources the archive lacks (anonymous, no token)
	bash ports/publish --check $(PUBLISH_ARGS) $(PORTS)

publish-plan: ## offline: give every shelf without a volume one, in ports/shelves
	bash ports/publish --plan $(PUBLISH_ARGS)

publish-describe: ## rewrite every archive release's title and notes from the index
	bash ports/publish --describe $(PUBLISH_ARGS)

publish-rehome: ## move archived files into their shelf's volume; run before and after pushing
	bash ports/publish --rehome $(PUBLISH_ARGS) $(PORTS)

publish-retire: ## delete every emptied legacy src-<shelf> release and its tag
	bash ports/publish --retire $(PUBLISH_ARGS)

publish-orphans: ## list index entries no recipe names any more
	bash ports/publish --orphans $(PUBLISH_ARGS)

need-tag:
	@test -n "$(TAG)" || { echo "ERROR: name the system tag, e.g. TAG=v0.2"; exit 1; }

freeze: need-tag ## attach sources.sha256 for TAG to its release
	bash ports/publish --freeze $(TAG) $(PUBLISH_ARGS)

release: need-tag ## create or update TAG's system release as a draft
	bash ports/publish --release $(TAG) $(PUBLISH_ARGS)

release-publish: need-tag ## make TAG's system release public and Latest
	bash ports/publish --release $(TAG) --publish $(PUBLISH_ARGS)

# Checks every port (or PORTUP_ARGS's own selection) for a newer upstream
# release. Needs network, curl and git (tags are read with ls-remote); never
# runs version control on the tree. See docs/kdos/05-developer/writing-ports.md
# "Checking for new versions", e.g. make updates PORTUP_ARGS="--check curl"
# --check exits 1 BY DESIGN when it finds an update, so a plain `make updates
# PORTUP_ARGS="--check zlib"` would otherwise print "Error 1" for a check
# that worked perfectly. 2 is the tool's own "unrecoverable" status (a revert
# itself failed) and anything else is a crash — both of those still have to
# fail the target.
updates: ## check ports for newer upstream releases (networked)
	@ports/update $(PORTUP_ARGS); rc=$$?; [ $$rc -le 1 ] || exit $$rc

# Rewriting the ISO while a VM boots from it corrupts that VM: QEMU reads the
# image lazily, so every block the guest has not cached yet turns into an I/O
# error (bash reports it as "<binary>: I/O error" on the next exec). Refuse,
# unless the developer insists.
check-iso-free:
	@if [ -f build/iso-build/kdos.iso ] && command -v fuser >/dev/null 2>&1 && \
	    fuser build/iso-build/kdos.iso >/dev/null 2>&1; then \
		echo "ERROR: build/iso-build/kdos.iso is open by another process — a running VM?"; \
		echo "       Rebuilding it now would give that guest I/O errors on anything it"; \
		echo "       has not already cached. Shut the VM down first, or override with:"; \
		echo "           make build ALLOW_ISO_IN_USE=1"; \
		test -n "$(ALLOW_ISO_IN_USE)" || exit 1; \
	fi

# A TTY is asked for only when stdin is one: without it docker refuses `-it`
# with "cannot attach stdin to a TTY-enabled container". With `-it` the
# container's stdout is a terminal even when make's is a file, so kdosbuild
# draws its TUI into it; a log or a CI run gets plain lines with no terminal
# on stdin (`make build </dev/null >build.log`) or with BUILD_ARGS=--plain.
DOCKER_TTY := $(shell test -t 0 && echo -it)

# HOW MUCH OF THE HOST THE BUILD TAKES. --cpu-shares is a weight, not a cap:
# an idle host gives the build every thread. Under contention the weight
# counts only against other containers and services; on a systemd cgroup-v2
# host the container sits under system.slice, which splits the CPU evenly
# with the user session. KDOS_JOBS, when set, is the job count every phase
# uses (make, cmake --build, cargo) and also becomes a --cpus cap, clamped to
# the host because docker refuses a --cpus above it, so cargo and go, which
# size themselves from the cgroup's cpu.max, follow it too; ninja is handed
# -jN by script/bin/ninja. Unset, the
# job count is computed inside the build by script/env/common.env, e.g.
#   make build KDOS_JOBS=6
KDOS_CPU_SHARES ?= 256

# The compiler cache for CMake ports inside the chroot, kept in build/ccache.
# A hit is byte-identical to a compile; 0 turns it off.
KDOS_CCACHE ?= 1

# The package store in build/pkgstore: 1 installs a port whose inputs match a
# stored package instead of building it, check builds it anyway and logs any
# difference, 0 (the default) is off. KDOS_PKG_STORE_MAX caps its size.
KDOS_PKG_STORE ?= 0
KDOS_PKG_STORE_MAX ?= 60G

build: check-iso-free ## build everything in the container (BUILD_ARGS narrows it)
	@case "$(KDOS_JOBS)" in *[!0-9]*|0*) echo "KDOS_JOBS must be a positive integer, got '$(KDOS_JOBS)'" >&2; exit 1;; esac
	mkdir -p build
	docker build -t os-dev .
	docker run --network none --cpu-shares=$(KDOS_CPU_SHARES) \
		$(if $(KDOS_JOBS),--cpus=$$(n=$$(nproc); j=$(KDOS_JOBS); [ $$j -lt $$n ] && echo $$j || echo $$n)) \
		--rm --privileged -e HOST_UID=$$(id -u) -e HOST_GID=$$(id -g) \
		-e KDOS_JOBS="$(KDOS_JOBS)" \
		-e KDOS_GIT_COMMIT="$$(git rev-parse --short HEAD 2>/dev/null)" \
		-e KDOS_GIT_DIRTY="$$(test -n "$$(git status --porcelain 2>/dev/null)" && echo 1 || echo 0)" \
		-e KDOS_GIT_DATE="$$(git log -1 --format=%cs 2>/dev/null)" \
		-e KDOS_GIT_TIME="$$(git log -1 --format=%ct 2>/dev/null)" \
		-e KDOS_ISO_SOURCES="$(KDOS_ISO_SOURCES)" \
		-e KDOS_PACK_KDOS="$(KDOS_PACK_KDOS)" \
		-e KDOS_MAKE_BINHOST="$(KDOS_MAKE_BINHOST)" \
		-e KDOS_ISO_COMP="$(KDOS_ISO_COMP)" \
		-e KDOS_CCACHE="$(KDOS_CCACHE)" \
		-e KDOS_PKG_STORE="$(KDOS_PKG_STORE)" \
		-e KDOS_PKG_STORE_MAX="$(KDOS_PKG_STORE_MAX)" \
		-v $$(pwd)/build:/workspace/build \
		-v $$(pwd)/src:/workspace/src:ro \
		-v $$(pwd)/fs:/workspace/fs:ro \
		-v $$(pwd)/script:/workspace/script:ro \
		-v $$(pwd)/ports:/workspace/ports:ro \
		-v $$(pwd)/docs:/workspace/docs:ro \
		-v $$(pwd)/testing:/workspace/testing:ro \
		-v $$(pwd)/Makefile:/workspace/Makefile:ro \
		-v $$(pwd)/Dockerfile:/workspace/Dockerfile:ro \
		$(DOCKER_TTY) os-dev script/kdosbuild.sh $(BUILD_ARGS)

snapshots: ## list the phase snapshots
	script/kdosbuild.sh --list

run: ## boot the ISO in QEMU (software graphics); creates a sparse 96G build/kdos.qcow2 if missing
	test -f build/iso-build/kdos.iso || { echo "ERROR: ISO not found at build/iso-build/kdos.iso — run 'make build' first"; exit 1; }
	test -r /usr/share/ovmf/OVMF.fd || { echo "ERROR: OVMF firmware not found at /usr/share/ovmf/OVMF.fd — install ovmf (debian: ovmf, arch: edk2-ovmf)"; exit 1; }
	test -c /dev/kvm 2>/dev/null || { echo "WARNING: /dev/kvm not found — QEMU will run without KVM (very slow)"; }
	test -f build/kdos.qcow2 || qemu-img create -f qcow2 build/kdos.qcow2 $(KDOS_DISK)
	qemu-system-x86_64 -enable-kvm -cpu host -smp $$(nproc) -m 4G -bios /usr/share/ovmf/OVMF.fd -cdrom build/iso-build/kdos.iso -serial stdio -drive file=build/kdos.qcow2,format=qcow2 -usb -device usb-tablet -vga none -device virtio-vga,xres=$(KDOS_XRES),yres=$(KDOS_YRES) -display gtk $$(testing/qemu-audio.sh) -netdev user,id=net0 -device virtio-net-pci,netdev=net0

rundisk: ## boot the installed disk image build/kdos.qcow2
	test -r /usr/share/ovmf/OVMF.fd || { echo "ERROR: OVMF firmware not found at /usr/share/ovmf/OVMF.fd"; exit 1; }
	test -c /dev/kvm 2>/dev/null || { echo "WARNING: /dev/kvm not found — QEMU will run without KVM (very slow)"; }
	test -f build/kdos.qcow2 || { echo "ERROR: disk image not found at build/kdos.qcow2 — run 'make run' first to create it"; exit 1; }
	qemu-system-x86_64 -enable-kvm -cpu host -smp $$(nproc) -m 4G -bios /usr/share/ovmf/OVMF.fd -serial stdio -drive file=build/kdos.qcow2,format=qcow2 -vga none -device virtio-vga,xres=$(KDOS_XRES),yres=$(KDOS_YRES) -display gtk $$(testing/qemu-audio.sh) -netdev user,id=net0 -device virtio-net-pci,netdev=net0

debug-boot: ## boot the kernel and initramfs directly, serial console
	test -f build/fs/boot/vmlinuz-kdos || { echo "ERROR: kernel not found at build/fs/boot/vmlinuz-kdos — run 'make build' first"; exit 1; }
	test -f build/iso-build/kdos.iso || { echo "ERROR: ISO not found at build/iso-build/kdos.iso — run 'make build' first"; exit 1; }
	qemu-system-x86_64 -smp $$(nproc) -m 4G -serial stdio \
		-kernel build/fs/boot/vmlinuz-kdos \
		-initrd build/fs/boot/initramfs.cpio.gz \
		-cdrom build/iso-build/kdos.iso \
		-append "root=/dev/ram0 rw console=tty0 console=ttyS0 quiet loglevel=3"

# HW-accelerated run via a containerized QEMU 10 (virgl+blob on the host GPU).
# `run`/`rundisk` above use the host's QEMU with software GL, which needs neither
# blob nor virgl support; these render kdos-comp on the real GPU. GPU and
# display flags (including gl=es — gl=on blanks the window) live in
# testing/qemu-hw/run.sh. Needs Docker + NVIDIA Container Toolkit.
run-hw: check-hw ## boot the ISO with GPU acceleration (containerised QEMU)
	KDOS_RES=$(KDOS_RES) KDOS_DISK=$(KDOS_DISK) testing/qemu-hw/run.sh iso

rundisk-hw: check-hw ## boot the disk image with GPU acceleration
	KDOS_RES=$(KDOS_RES) testing/qemu-hw/run.sh disk

check-hw:
	command -v docker >/dev/null || { echo "ERROR: docker not found — run-hw needs Docker + NVIDIA Container Toolkit"; exit 1; }
	docker info 2>/dev/null | grep -q ' nvidia' || { echo "WARNING: docker has no 'nvidia' runtime — virgl will fall back to software or fail"; }
	test -c /dev/udmabuf || { echo "WARNING: /dev/udmabuf not found — blob resources unavailable, kdos-comp will blank"; }

cleandisk: ## recreate an empty build/kdos.qcow2, sparse, 96G unless KDOS_DISK says otherwise
	qemu-img create -f qcow2 build/kdos.qcow2 $(KDOS_DISK)

# Wipe the build tree but keep build/snapshots, so a phase can still be
# restored, and build/ccache and build/pkgstore, whose contents stay valid for
# the next build. build/.snap-lineage goes with the tree it indexes, so the
# next snapshot is full.
#
# build/keys survives BOTH of these: a key is not a build artefact. Every image
# built beside kdos-release.key trusts its kdos-release.pub, and the key signs
# each `ports/publish --release`; lose it and no later release verifies there.
# linux-module-signing.pem signs the kernel's modules: lose it and the next
# kernel carries a new certificate, so modules from earlier builds load only as
# tainted. build/binhost-key is not kept: the next KDOS_MAKE_BINHOST=1 build
# makes a new binhost key, and a machine holding the old kdos-binhost.pub
# refuses the index it signs.
cleanbuild: ## wipe build/ except snapshots, ccache, pkgstore and keys
	test -d build && find build -mindepth 1 -maxdepth 1 \
		! -name snapshots ! -name keys ! -name ccache ! -name pkgstore \
		-exec rm -rf {} + || true

# Removes build/snapshots, build/ccache and build/pkgstore along with everything
# else. Not build/keys.
clean: ## wipe build/ except keys
	test -d build && find build -mindepth 1 -maxdepth 1 \
		! -name keys -exec rm -rf {} + || true

# The checks that stand in for a build. check runs the seven that need no built
# tree; depdrift, debuginfo, unowned, linkcheck and entrycheck read build/fs,
# which is root's, so they are run from a container or as root. SELFTEST_CC is
# the compiler selftest-asan uses. docscheck, anchorcheck and booknav cover
# both books, docs/kdos and docs/kfs; `make booknav-write` rewrites the
# chapter footers and KDOS from Scratch's contents that booknav checks.
# KFSCHECK_ARGS goes to kfscheck, e.g.
#   make kfscheck KFSCHECK_ARGS="--strict --base origin/main"
SELFTEST_CC ?= cc -fsanitize=address,undefined -g
KFSCHECK_ARGS ?=

check: preflight selftest docscheck anchorcheck booknav kfscheck phaseclosure ## preflight + selftest + docscheck + anchorcheck + booknav + kfscheck + phaseclosure

preflight: ## the build's wiring, minus the build (~2.5 min)
	testing/preflight.sh

selftest: ## the libk* libraries and their consumers (~2.5 min)
	testing/selftest.sh

selftest-asan: ## selftest under AddressSanitizer and UBSan (parsers)
	CC="$(SELFTEST_CC)" testing/selftest.sh

docscheck: ## both books: links, prose, page contract, the wiki skeleton, port counts
	bash testing/docscheck.sh

anchorcheck: ## every #fragment link in both books lands on a heading
	python3 testing/anchorcheck.py

booknav: ## every chapter footer, and KDOS from Scratch's contents, match the order
	python3 testing/booknav.py

booknav-write: ## rewrite the chapter footers and KDOS from Scratch's contents
	python3 testing/booknav.py --write

kfscheck: ## KDOS from Scratch: sidecars, notes, patch headers, data stamp, includes, render
	python3 testing/kfscheck.py $(KFSCHECK_ARGS)

phaseclosure: ## every phase installs exactly the ports its list names
	python3 testing/phaseclosure.py

depdrift: ## undeclared same-phase link dependencies in build/fs
	python3 testing/depdrift.py

debuginfo: ## fail when build/fs ships more than 1 MiB of DWARF
	python3 testing/debuginfo.py

unowned: ## files in build/fs no package and no fs/ path provides
	python3 testing/unowned.py

linkcheck: ## undeclared NEEDED owners, dead interpreters, mixed OpenSSL in build/fs
	python3 testing/linkcheck.py

entrycheck: ## dangling links, dead Exec=/Icon=, missing udev programs in build/fs
	python3 testing/entrycheck.py

# KDOS from Scratch, the second book (docs/kfs). `book` renders it into
# build/kfs: html/ one page per section, kfs.pdf and kfs-html.tar.zst; every
# package page is generated from its recipe on each run, so nothing it writes
# is committed. `book-data` rewrites the committed snapshot in docs/kfs/data
# from a built tree's kpkg database and build/snapshots/timings.json, and the
# recipe stamp beside it; run it after a build. `book-stamp` rewrites the
# stamp alone and needs no built tree: run it after any change that stales
# the stamp (a recipe, a kfs.md, a phase list, an included script).
# BUILDFS names that tree and, by default, the pandoc and typst that render
# the book, run through the tree's own loader so the book is made by KDOS's
# own binaries; KFS_PANDOC and KFS_TYPST replace them with any command, e.g.
#   make book BUILDFS=../kdos/build/fs
#   make book KFS_PANDOC=pandoc KFS_TYPST=typst
BUILDFS ?= build/fs
KFS_LOADER = $(BUILDFS)/usr/lib/libc.so --library-path $(BUILDFS)/usr/lib
KFS_PANDOC ?= $(KFS_LOADER) $(BUILDFS)/usr/bin/pandoc
KFS_TYPST ?= $(KFS_LOADER) $(BUILDFS)/usr/bin/typst

book: ## render KDOS from Scratch: build/kfs/html, kfs.pdf, kfs-html.tar.zst
	python3 script/book/kfsgen.py --out build/kfs/src
	python3 script/book/kfsrender.py --pandoc "$(KFS_PANDOC)" --typst "$(KFS_TYPST)" \
		--src build/kfs/src --out build/kfs --buildfs "$(BUILDFS)"

book-data: ## rewrite docs/kfs/data from BUILDFS's package database and timings
	python3 script/book/make_data.py --buildfs "$(BUILDFS)"

book-stamp: ## rewrite docs/kfs/data/recipes.stamp alone, from the checkout; no built tree
	python3 script/book/make_data.py --stamp-only

# The fast loop and the images it and the rig run in. QUICK names the ports
# (comma-separated); QUICK_ARGS goes to the rig after `--`, e.g.
#   make quick QUICK=kdos-shell QUICK_ARGS="--keys meta_l-ctrl-q --sleep 3 --shot build/shots/x.png"
QUICK ?=
QUICK_ARGS ?=

quick: ## rebuild QUICK's ports and patch them into a booted ISO
	@test -n "$(QUICK)" || { echo "ERROR: name the ports, e.g. QUICK=kdos-shell"; exit 1; }
	testing/quick.sh $(QUICK) -- $(QUICK_ARGS)

rig-image: ## build the rig container kdos-qemu-py:latest (networked, once)
	testing/rig-image.sh

devdeps-image: ## build kdos-devdeps:latest and run the suite in it
	testing/devdeps-image.sh

.PHONY: all help build check-iso-free snapshots run rundisk run-hw rundisk-hw check-hw debug-boot cleandisk cleanbuild clean fetch fetch-check updates publish publish-dry publish-check publish-plan publish-describe publish-rehome publish-retire publish-orphans need-tag freeze release release-publish check preflight selftest selftest-asan docscheck anchorcheck booknav booknav-write kfscheck phaseclosure depdrift debuginfo unowned linkcheck entrycheck book book-data book-stamp quick rig-image devdeps-image
