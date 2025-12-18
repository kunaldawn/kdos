all: build

fetch:
	bash script/00_fetch.sh

build:
	mkdir -p build
	docker build -t os-dev .
	docker run --rm -u $$(id -u):$$(id -g) -v $$(pwd)/build:/workspace/build -it os-dev bash script/build.sh

run:
	qemu-system-x86_64 -enable-kvm -cpu host -m 4G -bios /usr/share/ovmf/OVMF.fd -cdrom build/kdos.iso -nographic

clean:
	rm -rf build

.PHONY: all build run clean fetch
