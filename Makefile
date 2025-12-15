all: build

build:
	mkdir -p build
	docker build -t os-dev .
	docker run -u $$(id -u):$$(id -g) -v $$(pwd)/build:/workspace/build -it os-dev

run:
	qemu-system-x86_64 -kernel build/bzImage -initrd build/init.cpio -append "root=/dev/ram0 console=ttyS0" -nographic

clean:
	rm -rf build

.PHONY: all build run clean
