all: build

fetch:
	bash ports/fetch

build:
	mkdir -p build
	docker build -t os-dev .
	docker run --rm --privileged -e HOST_UID=$$(id -u) -e HOST_GID=$$(id -g) -v $$(pwd)/build:/workspace/build -it os-dev bash script/build.sh

run:
	qemu-system-x86_64 -enable-kvm -cpu host -m 4G -bios /usr/share/ovmf/OVMF.fd -cdrom build/kdos.iso -nographic

clean:
	rm -rf build

.PHONY: all build run clean fetch
