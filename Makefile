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

fetch:
	bash ports/fetch

build:
	mkdir -p build
	docker build -t os-dev .
	docker run --rm --privileged -e HOST_UID=$$(id -u) -e HOST_GID=$$(id -g) -v $$(pwd)/build:/workspace/build -it os-dev python3 script/build.py

run:
	test -f build/kdos.qcow2 || qemu-img create -f qcow2 build/kdos.qcow2 20G
	qemu-system-x86_64 -enable-kvm -cpu host -m 4G -bios /usr/share/ovmf/OVMF.fd -cdrom build/iso-build/kdos.iso -serial stdio -drive file=build/kdos.qcow2,format=qcow2 -usb -device usb-tablet

rundisk:
	qemu-system-x86_64 -enable-kvm -cpu host -m 4G -bios /usr/share/ovmf/OVMF.fd -serial stdio -drive file=build/kdos.qcow2,format=qcow2

cleandisk:
	qemu-img create -f qcow2 build/kdos.qcow2 20G

clean:
	rm -rf build

.PHONY: all build run rundisk cleandisk clean fetch
