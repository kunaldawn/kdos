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
#   make build BUILD_ARGS="--restore phase2"
#   make build BUILD_ARGS=--fresh
#   make build BUILD_ARGS=--no-snapshot
BUILD_ARGS ?=

fetch:
	bash ports/fetch
	@test -f ports/appbox/appbox.tar || echo "hint: 'make fetch-apps' builds the offline alien-app image (needs network + docker/podman)"

# Checks every port (or PORTUP_ARGS's own selection) for a newer upstream
# release. Needs network; never touches git. See CLAUDE.md's "kdos-portup"
# section, e.g. make updates PORTUP_ARGS="--check curl"
# --check exits 1 BY DESIGN when it finds an update, so a plain `make updates
# PORTUP_ARGS="--check zlib"` would otherwise print "Error 1" for a check
# that worked perfectly. 2 is the tool's own "unrecoverable" status (a revert
# itself failed) and anything else is a crash — both of those still have to
# fail the target.
updates:
	@ports/update $(PORTUP_ARGS); rc=$$?; [ $$rc -le 1 ] || exit $$rc

# Build the kdos-apps distrobox image on the host and stash it for the
# (network-less) ISO build to bake in. See ports/appbox/.
fetch-apps:
	bash ports/appbox/fetch

# Build the PACK SET on the host and stash it for the (network-less) ISO build
# to place on the medium. An application is one signed EROFS image with a KDOS
# footer on the end; the runtimes underneath are shared, so editing one
# application rewrites one file rather than a 485 MB blob.
#
# ROOT, because mkfs.erofs preserves the overlay whiteouts and the
# trusted.overlay xattrs only as root, and podman's store only writes real ones
# as root. See ports/appbox/packs.
fetch-packs:
	sudo -E bash ports/appbox/packs

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

# `-it` unconditionally made `make build` impossible without a terminal —
# docker refuses with "cannot attach stdin to a TTY-enabled container", which is
# exactly the case kdosbuild's headless mode exists for (a non-tty stdout gets
# plain lines instead of the TUI). Ask for a TTY only when there is one, so a
# build can be logged to a file or run from CI.
DOCKER_TTY := $(shell test -t 0 && echo -it)

build: check-iso-free
	mkdir -p build
	docker build -t os-dev .
	docker run --network none --cpus="10" --rm --privileged -e HOST_UID=$$(id -u) -e HOST_GID=$$(id -g) \
		-e KDOS_GIT_COMMIT="$$(git rev-parse --short HEAD 2>/dev/null)" \
		-e KDOS_GIT_DIRTY="$$(test -n "$$(git status --porcelain 2>/dev/null)" && echo 1 || echo 0)" \
		-e KDOS_ISO_SOURCES="$(KDOS_ISO_SOURCES)" \
		-e KDOS_PACK_KDOS="$(KDOS_PACK_KDOS)" \
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
	qemu-system-x86_64 -enable-kvm -cpu host -smp $$(nproc) -m 4G -bios /usr/share/ovmf/OVMF.fd -cdrom build/iso-build/kdos.iso -serial stdio -drive file=build/kdos.qcow2,format=qcow2 -usb -device usb-tablet -vga none -device virtio-vga -display gtk $$(testing/qemu-audio.sh) -netdev user,id=net0 -device virtio-net-pci,netdev=net0

rundisk:
	test -r /usr/share/ovmf/OVMF.fd || { echo "ERROR: OVMF firmware not found at /usr/share/ovmf/OVMF.fd"; exit 1; }
	test -c /dev/kvm 2>/dev/null || { echo "WARNING: /dev/kvm not found — QEMU will run without KVM (very slow)"; }
	test -f build/kdos.qcow2 || { echo "ERROR: disk image not found at build/kdos.qcow2 — run 'make run' first to create it"; exit 1; }
	qemu-system-x86_64 -enable-kvm -cpu host -smp $$(nproc) -m 4G -bios /usr/share/ovmf/OVMF.fd -serial stdio -drive file=build/kdos.qcow2,format=qcow2 -vga none -device virtio-vga -display gtk $$(testing/qemu-audio.sh) -netdev user,id=net0 -device virtio-net-pci,netdev=net0

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
# software-GL for the shell; these render the Qt shell on the real GPU. GPU and
# display flags (including gl=es — gl=on blanks the window) live in
# testing/qemu-hw/run.sh. Needs Docker + NVIDIA Container Toolkit.
run-hw: check-hw
	testing/qemu-hw/run.sh iso

rundisk-hw: check-hw
	testing/qemu-hw/run.sh disk

check-hw:
	command -v docker >/dev/null || { echo "ERROR: docker not found — run-hw needs Docker + NVIDIA Container Toolkit"; exit 1; }
	docker info 2>/dev/null | grep -q ' nvidia' || { echo "WARNING: docker has no 'nvidia' runtime — virgl will fall back to software or fail"; }
	test -c /dev/udmabuf || { echo "WARNING: /dev/udmabuf not found — blob resources unavailable, the Qt shell will blank"; }

cleandisk:
	qemu-img create -f qcow2 build/kdos.qcow2 20G

# Wipe the build tree but keep build/snapshots, so a phase can still be restored.
cleanbuild:
	test -d build && find build -mindepth 1 -maxdepth 1 ! -name snapshots -exec rm -rf {} + || true

# Removes build/snapshots along with everything else.
clean:
	rm -rf build

.PHONY: fetch-packs all build check-iso-free snapshots run rundisk run-hw rundisk-hw check-hw debug-boot cleandisk cleanbuild clean fetch updates
