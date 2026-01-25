#!/bin/bash

# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# Helper function to extract source from port directory
# Usage: extract_port_source <port_name>
# Returns: Path to extracted source directory

extract_port_source() {
    local port=$1
    local portdir="$WORKSPACE/ports/core/$port"
    
    cd "$portdir" || {
        echo "ERROR: Port not found: $port" >&2
        return 1
    }
    
    # Source kpkgbuild to get package info
    unset name version source
    . ./kpkgbuild || {
        echo "ERROR: Failed to source kpkgbuild for $port" >&2
        return 1
    }
    
    # Find the tarball in port directory
    local tarball=""

    local src_idx=0
    for src in $source; do
        case $src in
            *::*) tarball="${src%%::*}" ;;
            http://*|https://*|ftp://*) 
                # Default to basename
                tarball=$(basename "$src")
                
                # If first source, check for extension match to use standardized name
                if [ $src_idx -eq 0 ]; then
                    url="$src"
                    if [[ "$url" =~ \.tar\.gz$ || "$url" =~ \.tgz$ ]]; then ext="tar.gz"
                    elif [[ "$url" =~ \.tar\.bz2$ || "$url" =~ \.tbz2$ ]]; then ext="tar.bz2"
                    elif [[ "$url" =~ \.tar\.xz$ || "$url" =~ \.txz$ ]]; then ext="tar.xz"
                    elif [[ "$url" =~ \.tar\.zst$ ]]; then ext="tar.zst"
                    elif [[ "$url" =~ \.zip$ ]]; then ext="zip"
                    else ext=""; fi
                    
                    if [ -n "$ext" ]; then
                        tarball="$name-$version.$ext"
                    fi
                fi
                ;;
            *) tarball="$src" ;;
        esac
        
        # Check if it's a tarball and exists
        case $tarball in
            *.tar.*|*.tgz|*.tbz2|*.txz|*.zip)
                if [ -f "$portdir/$tarball" ]; then
                    break
                fi
                ;;
        esac
        src_idx=$((src_idx + 1))
    done
    
    if [ -z "$tarball" ] || [ ! -f "$portdir/$tarball" ]; then
        echo "ERROR: Source tarball not found for $port" >&2
        return 1
    fi
    
    # Extract to build directory
    local extract_dir="$BUILD_DIR/tmp"
    mkdir -p "$extract_dir"
    
    echo "Extracting $tarball..." >&2
    tar -xf "$portdir/$tarball" -C "$extract_dir" || {
        echo "ERROR: Failed to extract $tarball" >&2
        return 1
    }
    
    echo "$extract_dir/$name-$version"
}

kpkg_install() {
    echo ">>> kpkg installing $@..." >&2
    
    # Add kpkg tools to PATH
    export PATH="$SRC_DIR/kpkg:$PATH"
    
    # Configure for host execution targeting sysroot
    export KPKG_CONF="$SCRIPT_DIR/phase1/kpkg_host.conf"
    export KPKG_ROOT="$SYSROOT"
    
    # Run kpkg
    kpkg install --force "$@"
}

get_port_version() {
    local port=$1
    local portdir="$WORKSPACE/ports/core/$port"
    
    if [ ! -d "$portdir" ]; then
        echo "ERROR: Port not found: $port" >&2
        return 1
    fi
    
    unset name version source
    . "$portdir/kpkgbuild" || {
        echo "ERROR: Failed to source kpkgbuild for $port" >&2
        return 1
    }
    
    echo "$version"
}
