all: build

fetch:
	bash script/00_fetch.sh

build:
	mkdir -p build
	docker build -t os-dev .
	docker run --rm -u $$(id -u):$$(id -g) -v $$(pwd)/build:/workspace/build -it os-dev bash script/build.sh

run:
	qemu-system-x86_64 -m 4G -kernel build/bzImage -initrd build/init.cpio -append "root=/dev/ram0 console=ttyS0" -nographic

clean:
	rm -rf build

.PHONY: all build run clean fetch
