#!/bin/bash
set -e
source script/env.sh

BOOTLOADER_BUILD_DIR=$BUILD_DIR/bootloaders
mkdir -p $BOOTLOADER_BUILD_DIR

if [ -f "$BOOTLOADER_BUILD_DIR/refind-installed" ]; then
    exit 0
fi

# Define flags
EFI_CFLAGS="-g -O2 -Wall -Wextra -Wno-error \
    -funsigned-char -fshort-wchar -fno-strict-aliasing \
    -ffreestanding -fno-stack-protector -fno-stack-check \
    -fno-merge-all-constants \
    -mno-red-zone \
    -DCONFIG_x86_64 -std=c11 \
    -mno-avx -D__DEFINED_wchar_t -Dwchar_t=__UINT16_TYPE__ \
    -fPIC"

echo ">>> Building rEFInd..."
rm -rf $BOOTLOADER_BUILD_DIR/refind-$REFIND_VER
tar -xf $SRC_DIR/refind-src-$REFIND_VER.tar.gz -C $BOOTLOADER_BUILD_DIR
cd $BOOTLOADER_BUILD_DIR/refind-$REFIND_VER

# rEFInd needs to know where gnu-efi is.
_EFIINC=$BOOTLOADER_BUILD_DIR/gnu-efi-$GNU_EFI_VER/inc
_EFILIB=$BOOTLOADER_BUILD_DIR/gnu-efi-$GNU_EFI_VER/x86_64/lib
_GNUEFILIB=$BOOTLOADER_BUILD_DIR/gnu-efi-$GNU_EFI_VER/x86_64/gnuefi

_LDSCRIPT=$BOOTLOADER_BUILD_DIR/gnu-efi-$GNU_EFI_VER/gnuefi/elf_x86_64_efi.lds
_CRTOBJ=$_GNUEFILIB/crt0-efi-x86_64.o

make -C "$BOOTLOADER_BUILD_DIR/refind-$REFIND_VER" \
    CC="x86_64-kdos-linux-musl-gcc" \
    AS="x86_64-kdos-linux-musl-as" \
    LD="x86_64-kdos-linux-musl-ld" \
    AR="x86_64-kdos-linux-musl-ar" \
    RANLIB="x86_64-kdos-linux-musl-ranlib" \
    OBJCOPY="x86_64-kdos-linux-musl-objcopy" \
    ARCH=x86_64 \
    EFIINC="$_EFIINC" \
    EFILIB="$_EFILIB" \
    GNUEFILIB="$_GNUEFILIB" \
    GNUEFI_LDSCRIPT="$_LDSCRIPT" \
    CRTOBJS="$_CRTOBJ" \
    CFLAGS="${EFI_CFLAGS}" \
    gnuefi || exit 1

# Build ISO9660 Driver
echo ">>> Building rEFInd ISO9660 driver..."
make -C "$BOOTLOADER_BUILD_DIR/refind-$REFIND_VER/filesystems" \
    CC="x86_64-kdos-linux-musl-gcc" \
    AS="x86_64-kdos-linux-musl-as" \
    LD="x86_64-kdos-linux-musl-ld" \
    AR="x86_64-kdos-linux-musl-ar" \
    RANLIB="x86_64-kdos-linux-musl-ranlib" \
    OBJCOPY="x86_64-kdos-linux-musl-objcopy" \
    ARCH=x86_64 \
    EFIINC="$_EFIINC" \
    EFILIB="$_EFILIB" \
    GNUEFILIB="$_GNUEFILIB" \
    GNUEFI_LDSCRIPT="$_LDSCRIPT" \
    CRTOBJS="$_CRTOBJ" \
    CFLAGS="${EFI_CFLAGS}" \
    iso9660_gnuefi || exit 1

touch "$BOOTLOADER_BUILD_DIR/refind-installed"
