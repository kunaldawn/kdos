#!/bin/bash
set -e
source script/env.sh

BOOTLOADER_BUILD_DIR=$BUILD_DIR/bootloaders
mkdir -p $BOOTLOADER_BUILD_DIR

if [ -f "$BOOTLOADER_BUILD_DIR/gnu-efi-installed" ]; then
    exit 0
fi

# Define flags (see original script)
EFI_CFLAGS="-g -O2 -Wall -Wextra -Wno-error \
    -funsigned-char -fshort-wchar -fno-strict-aliasing \
    -ffreestanding -fno-stack-protector -fno-stack-check \
    -fno-merge-all-constants \
    -mno-red-zone \
    -DCONFIG_x86_64 -std=c11 \
    -mno-avx -D__DEFINED_wchar_t -Dwchar_t=__UINT16_TYPE__ \
    -fPIC"

echo ">>> Building gnu-efi..."
rm -rf $BOOTLOADER_BUILD_DIR/gnu-efi-$GNU_EFI_VER
tar -xf $SRC_DIR/gnu-efi-$GNU_EFI_VER.tar.bz2 -C $BOOTLOADER_BUILD_DIR
cd $BOOTLOADER_BUILD_DIR/gnu-efi-$GNU_EFI_VER

GNU_EFI_DIR="$BOOTLOADER_BUILD_DIR/gnu-efi-$GNU_EFI_VER"

make -C "$GNU_EFI_DIR" \
    CC="x86_64-kdos-linux-musl-gcc" \
    AR="x86_64-kdos-linux-musl-ar" \
    LD="x86_64-kdos-linux-musl-ld" \
    OBJCOPY="x86_64-kdos-linux-musl-objcopy" \
    ARCH=x86_64 \
    CFLAGS="${EFI_CFLAGS} -I$GNU_EFI_DIR/lib -I$GNU_EFI_DIR/gnuefi -I$GNU_EFI_DIR/inc -I$GNU_EFI_DIR/inc/x86_64 -I$GNU_EFI_DIR/inc/protocol" \
    ASFLAGS="${EFI_CFLAGS}" \
    CPPFLAGS="${EFI_CFLAGS}" \
    lib gnuefi || exit 1

touch "$BOOTLOADER_BUILD_DIR/gnu-efi-installed"
