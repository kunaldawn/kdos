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

# Extra flags for script/build.py, e.g.
#   make build BUILD_ARGS="--restore phase2"
#   make build BUILD_ARGS=--fresh
#   make build BUILD_ARGS=--no-snapshot
BUILD_ARGS ?=

fetch:
	bash ports/fetch

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

build: check-iso-free
	mkdir -p build
	docker build -t os-dev .
	docker run --network none --cpus="10" --rm --privileged -e HOST_UID=$$(id -u) -e HOST_GID=$$(id -g) \
		-e KDOS_GIT_COMMIT="$$(git rev-parse --short HEAD 2>/dev/null)" \
		-e KDOS_GIT_DIRTY="$$(test -n "$$(git status --porcelain 2>/dev/null)" && echo 1 || echo 0)" \
		-v $$(pwd)/build:/workspace/build \
		-v $$(pwd)/src:/workspace/src:ro \
		-v $$(pwd)/fs:/workspace/fs:ro \
		-v $$(pwd)/script:/workspace/script:ro \
		-v $$(pwd)/ports:/workspace/ports:ro \
		-it os-dev python3 script/build.py $(BUILD_ARGS)

snapshots:
	python3 script/build.py --list

run:
	test -f build/iso-build/kdos.iso || { echo "ERROR: ISO not found at build/iso-build/kdos.iso — run 'make build' first"; exit 1; }
	test -r /usr/share/ovmf/OVMF.fd || { echo "ERROR: OVMF firmware not found at /usr/share/ovmf/OVMF.fd — install ovmf (debian: ovmf, arch: edk2-ovmf)"; exit 1; }
	test -c /dev/kvm 2>/dev/null || { echo "WARNING: /dev/kvm not found — QEMU will run without KVM (very slow)"; }
	test -f build/kdos.qcow2 || qemu-img create -f qcow2 build/kdos.qcow2 20G
	qemu-system-x86_64 -enable-kvm -cpu host -m 4G -bios /usr/share/ovmf/OVMF.fd -cdrom build/iso-build/kdos.iso -serial stdio -drive file=build/kdos.qcow2,format=qcow2 -usb -device usb-tablet -vga none -device virtio-vga -display gtk -netdev user,id=net0 -device virtio-net-pci,netdev=net0

rundisk:
	test -r /usr/share/ovmf/OVMF.fd || { echo "ERROR: OVMF firmware not found at /usr/share/ovmf/OVMF.fd"; exit 1; }
	test -c /dev/kvm 2>/dev/null || { echo "WARNING: /dev/kvm not found — QEMU will run without KVM (very slow)"; }
	test -f build/kdos.qcow2 || { echo "ERROR: disk image not found at build/kdos.qcow2 — run 'make run' first to create it"; exit 1; }
	qemu-system-x86_64 -enable-kvm -cpu host -m 4G -bios /usr/share/ovmf/OVMF.fd -serial stdio -drive file=build/kdos.qcow2,format=qcow2 -vga none -device virtio-vga -display gtk -netdev user,id=net0 -device virtio-net-pci,netdev=net0

debug-boot:
	test -f build/fs/boot/vmlinuz-kdos || { echo "ERROR: kernel not found at build/fs/boot/vmlinuz-kdos — run 'make build' first"; exit 1; }
	test -f build/iso-build/kdos.iso || { echo "ERROR: ISO not found at build/iso-build/kdos.iso — run 'make build' first"; exit 1; }
	qemu-system-x86_64 -m 4G -serial stdio \
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

.PHONY: all build check-iso-free snapshots run rundisk run-hw rundisk-hw check-hw debug-boot cleandisk cleanbuild clean fetch
