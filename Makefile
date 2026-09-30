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

# Extra flags for the orchestrator, e.g.
#   make build BUILD_ARGS="--restore 20_selfhost"
#   make build BUILD_ARGS=--fresh
#   make build BUILD_ARGS=--no-snapshot
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

# The only networked step: every port's sources from the archive, the cache or
# upstream, verified against the recipe. `make build` never downloads, so run
# this once after a clone and after any recipe change. fetch-check is offline
# and exits 1 naming each archived source missing or corrupt. FETCH_JOBS=N
# works on N ports at once; the default, 1, is one after the other.
FETCH_JOBS ?= 1

fetch:
	FETCH_JOBS=$(FETCH_JOBS) bash ports/fetch

fetch-check:
	FETCH_JOBS=$(FETCH_JOBS) bash ports/fetch --check

# Checks every port (or PORTUP_ARGS's own selection) for a newer upstream
# release. Needs network, curl and git (tags are read with ls-remote); never
# runs version control on the tree. See docs/kdos/05-developer/writing-ports.md
# "Checking for new versions", e.g. make updates PORTUP_ARGS="--check curl"
# --check exits 1 BY DESIGN when it finds an update, so a plain `make updates
# PORTUP_ARGS="--check zlib"` would otherwise print "Error 1" for a check
# that worked perfectly. 2 is the tool's own "unrecoverable" status (a revert
# itself failed) and anything else is a crash — both of those still have to
# fail the target.
updates:
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

# A TTY is asked for only when there is one: with no terminal, docker refuses
# `-it` with "cannot attach stdin to a TTY-enabled container", and no terminal
# is exactly the case kdosbuild's headless mode exists for (a non-tty stdout
# gets plain lines instead of the TUI). So a build can be logged to a file or
# run from CI.
DOCKER_TTY := $(shell test -t 0 && echo -it)

# HOW MUCH OF THE HOST THE BUILD TAKES. --cpu-shares is a weight, not a cap:
# an idle host gives the build every thread. Under contention the weight
# counts only against other containers and services; on a systemd cgroup-v2
# host the container sits under system.slice, which splits the CPU evenly
# with the user session. KDOS_JOBS, when set, is the job count every phase
# uses (make, cmake --build, cargo) and also becomes a --cpus cap, clamped to
# the host because docker refuses a --cpus above it, so ninja, cargo and go,
# which size themselves from the cgroup's cpu.max, follow it too. Unset, the
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

build: check-iso-free
	@case "$(KDOS_JOBS)" in *[!0-9]*|0*) echo "KDOS_JOBS must be a positive integer, got '$(KDOS_JOBS)'" >&2; exit 1;; esac
	mkdir -p build
	docker build -t os-dev .
	docker run --network none --cpu-shares=$(KDOS_CPU_SHARES) \
		$(if $(KDOS_JOBS),--cpus=$$(n=$$(nproc); j=$(KDOS_JOBS); [ $$j -lt $$n ] && echo $$j || echo $$n)) \
		--rm --privileged -e HOST_UID=$$(id -u) -e HOST_GID=$$(id -g) \
		-e KDOS_JOBS="$(KDOS_JOBS)" \
		-e KDOS_GIT_COMMIT="$$(git rev-parse --short HEAD 2>/dev/null)" \
		-e KDOS_GIT_DIRTY="$$(test -n "$$(git status --porcelain 2>/dev/null)" && echo 1 || echo 0)" \
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
		$(DOCKER_TTY) os-dev script/kdosbuild.sh $(BUILD_ARGS)

snapshots:
	script/kdosbuild.sh --list

run:
	test -f build/iso-build/kdos.iso || { echo "ERROR: ISO not found at build/iso-build/kdos.iso — run 'make build' first"; exit 1; }
	test -r /usr/share/ovmf/OVMF.fd || { echo "ERROR: OVMF firmware not found at /usr/share/ovmf/OVMF.fd — install ovmf (debian: ovmf, arch: edk2-ovmf)"; exit 1; }
	test -c /dev/kvm 2>/dev/null || { echo "WARNING: /dev/kvm not found — QEMU will run without KVM (very slow)"; }
	test -f build/kdos.qcow2 || qemu-img create -f qcow2 build/kdos.qcow2 20G
	qemu-system-x86_64 -enable-kvm -cpu host -smp $$(nproc) -m 4G -bios /usr/share/ovmf/OVMF.fd -cdrom build/iso-build/kdos.iso -serial stdio -drive file=build/kdos.qcow2,format=qcow2 -usb -device usb-tablet -vga none -device virtio-vga,xres=$(KDOS_XRES),yres=$(KDOS_YRES) -display gtk $$(testing/qemu-audio.sh) -netdev user,id=net0 -device virtio-net-pci,netdev=net0

rundisk:
	test -r /usr/share/ovmf/OVMF.fd || { echo "ERROR: OVMF firmware not found at /usr/share/ovmf/OVMF.fd"; exit 1; }
	test -c /dev/kvm 2>/dev/null || { echo "WARNING: /dev/kvm not found — QEMU will run without KVM (very slow)"; }
	test -f build/kdos.qcow2 || { echo "ERROR: disk image not found at build/kdos.qcow2 — run 'make run' first to create it"; exit 1; }
	qemu-system-x86_64 -enable-kvm -cpu host -smp $$(nproc) -m 4G -bios /usr/share/ovmf/OVMF.fd -serial stdio -drive file=build/kdos.qcow2,format=qcow2 -vga none -device virtio-vga,xres=$(KDOS_XRES),yres=$(KDOS_YRES) -display gtk $$(testing/qemu-audio.sh) -netdev user,id=net0 -device virtio-net-pci,netdev=net0

debug-boot:
	test -f build/fs/boot/vmlinuz-kdos || { echo "ERROR: kernel not found at build/fs/boot/vmlinuz-kdos — run 'make build' first"; exit 1; }
	test -f build/iso-build/kdos.iso || { echo "ERROR: ISO not found at build/iso-build/kdos.iso — run 'make build' first"; exit 1; }
	qemu-system-x86_64 -smp $$(nproc) -m 4G -serial stdio \
		-kernel build/fs/boot/vmlinuz-kdos \
		-initrd build/fs/boot/initramfs.cpio.gz \
		-cdrom build/iso-build/kdos.iso \
		-append "root=/dev/ram0 rw console=tty0 console=ttyS0 quiet loglevel=3"

# HW-accelerated run via a containerized QEMU 10 (virgl+blob on the host GPU).
# The host's packaged QEMU is 8.2.2 (no blob+virgl) so `run`/`rundisk` above stay
# software-GL for the desktop; these render kdos-comp on the real GPU. GPU and
# display flags (including gl=es — gl=on blanks the window) live in
# testing/qemu-hw/run.sh. Needs Docker + NVIDIA Container Toolkit.
run-hw: check-hw
	KDOS_RES=$(KDOS_RES) testing/qemu-hw/run.sh iso

rundisk-hw: check-hw
	KDOS_RES=$(KDOS_RES) testing/qemu-hw/run.sh disk

check-hw:
	command -v docker >/dev/null || { echo "ERROR: docker not found — run-hw needs Docker + NVIDIA Container Toolkit"; exit 1; }
	docker info 2>/dev/null | grep -q ' nvidia' || { echo "WARNING: docker has no 'nvidia' runtime — virgl will fall back to software or fail"; }
	test -c /dev/udmabuf || { echo "WARNING: /dev/udmabuf not found — blob resources unavailable, kdos-comp will blank"; }

cleandisk:
	qemu-img create -f qcow2 build/kdos.qcow2 20G

# Wipe the build tree but keep build/snapshots, so a phase can still be
# restored, and build/ccache and build/pkgstore, whose contents stay valid for
# the next build.
#
# build/keys survives BOTH of these, and that is deliberate: the pack signing
# key lives there and a key is not a build artefact. Lose it and every later
# bake is unsigned while every medium already written keeps trusting a key you
# can no longer sign with — libksig has no revocation and no expiry. Set
# KDOS_PACK_KEY to keep it outside the tree entirely.
cleanbuild:
	test -d build && find build -mindepth 1 -maxdepth 1 \
		! -name snapshots ! -name keys ! -name ccache ! -name pkgstore \
		-exec rm -rf {} + || true

# Removes build/snapshots, build/ccache and build/pkgstore along with everything
# else. Not build/keys.
clean:
	test -d build && find build -mindepth 1 -maxdepth 1 \
		! -name keys -exec rm -rf {} + || true

.PHONY: all build check-iso-free snapshots run rundisk run-hw rundisk-hw check-hw debug-boot cleandisk cleanbuild clean fetch fetch-check updates
