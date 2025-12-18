#!/bin/bash
set -e
source script/env.sh

echo ">>> Building Bootloaders..."

BOOTLOADER_BUILD_DIR=$BUILD_DIR/bootloaders
mkdir -p $BOOTLOADER_BUILD_DIR

# We must pass the FULL set of flags because setting CFLAGS overrides Make.defaults entirely.
# We include all standard flags from Make.defaults but change -Werror to -Wno-error.
EFI_CFLAGS="-g -O2 -Wall -Wextra -Wno-error \
    -funsigned-char -fshort-wchar -fno-strict-aliasing \
    -ffreestanding -fno-stack-protector -fno-stack-check \
    -fno-merge-all-constants \
    -mno-red-zone \
    -DCONFIG_x86_64 -std=c11 \
    -mno-avx -D__DEFINED_wchar_t -Dwchar_t=__UINT16_TYPE__ \
    -fPIC"

# -----------------------------------------------------------------------------
# GNU-EFI
# -----------------------------------------------------------------------------
if [ ! -f "$BOOTLOADER_BUILD_DIR/gnu-efi-installed" ]; then
    echo ">>> Building gnu-efi..."
    rm -rf $BOOTLOADER_BUILD_DIR/gnu-efi-$GNU_EFI_VER
    tar -xf $SRC_DIR/gnu-efi-$GNU_EFI_VER.tar.bz2 -C $BOOTLOADER_BUILD_DIR
    cd $BOOTLOADER_BUILD_DIR/gnu-efi-$GNU_EFI_VER
    


# Export paths for rEFInd to pick up (rEFInd's Makefile uses these if set)
# But we also pass them explicitly to make command to be sure
# Note: rEFInd uses 'EFIINC', 'EFILIB', 'GNUEFILIB'
# Define absolute paths for build
    GNU_EFI_DIR="$BOOTLOADER_BUILD_DIR/gnu-efi-$GNU_EFI_VER"

    # Build gnu-efi
    echo ">>> Building gnu-efi..."
    # We use absolute path for -C to be safe, even though we cd'd in.
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
else
    echo "gnu-efi already built."
fi

# -----------------------------------------------------------------------------
# rEFInd
# -----------------------------------------------------------------------------
if [ ! -f "$BOOTLOADER_BUILD_DIR/refind-installed" ]; then
    echo ">>> Building rEFInd..."
    rm -rf $BOOTLOADER_BUILD_DIR/refind-$REFIND_VER
    tar -xf $SRC_DIR/refind-src-$REFIND_VER.tar.gz -C $BOOTLOADER_BUILD_DIR
    cd $BOOTLOADER_BUILD_DIR/refind-$REFIND_VER
    
    # rEFInd needs to know where gnu-efi is.
    # It uses EFIINC, EFILIB, GNUEFILIB
    _EFIINC=$BOOTLOADER_BUILD_DIR/gnu-efi-$GNU_EFI_VER/inc
    _EFILIB=$BOOTLOADER_BUILD_DIR/gnu-efi-$GNU_EFI_VER/x86_64/lib
    _GNUEFILIB=$BOOTLOADER_BUILD_DIR/gnu-efi-$GNU_EFI_VER/x86_64/gnuefi
    
    # We also need to explicitly locate the linker script and CRT object
    # because gnu-efi build places them in separate directories.
    _LDSCRIPT=$BOOTLOADER_BUILD_DIR/gnu-efi-$GNU_EFI_VER/gnuefi/elf_x86_64_efi.lds
    _CRTOBJ=$_GNUEFILIB/crt0-efi-x86_64.o
    
    # Build rEFInd
    echo ">>> Building rEFInd..."
    # Use variables defined above (_EFIINC, _EFILIB, _GNUEFILIB)
    
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
else
    echo "rEFInd already built."
fi
