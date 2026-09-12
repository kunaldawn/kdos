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

# Read a recipe's metadata.
#
# A kpkgbuild is `key = value` and is NOT a shell script any more, so it cannot
# be sourced. It does not need to be: the values are a subset of shell
# assignment syntax, so bash performs `$name`, `${version%.*}` and the rest
# itself — which is exactly what the recipes were written against.
#
# Only the keys this file uses are evaluated. `description` and `homepage` are
# free text that could contain a backtick, and there is no reason to let them
# near an eval.
read_recipe() {
    local file=$1 line key val
    unset name version release source vendoring
    [ -f "$file" ] || return 1

    while IFS= read -r line || [ -n "$line" ]; do
        case "$line" in
            '#'*|'') continue ;;
            *=*) ;;
            *) continue ;;
        esac
        key=${line%%=*}
        key=${key%"${key##*[![:space:]]}"}
        val=${line#*=}
        val=${val#"${val%%[![:space:]]*}"}

        case "$key" in
            name|version|release|vendoring)
                eval "$key=\"$val\"" ;;
            source)
                # Repeating the key appends; the extractor splits on space.
                eval "source=\"\${source:+\$source }$val\"" ;;
            description|homepage|depends|pypackages)
                # FREE TEXT. imagemagick's description is
                #   ImageMagick — convert, edit, ... (provides `convert`)
                # and those backticks ran ImageMagick's own `convert` the
                # first time this loop evaluated every key it did not
                # recognise. Nothing here needs them, so nothing here
                # evaluates them.
                continue ;;
            [A-Za-z_]*)
                # A recipe helper — `_tag`, `vrsn`. Declared before `source`,
                # which is what lets the URL below refer to it.
                case "$key" in
                    *[!A-Za-z0-9_]*) continue ;;
                esac
                eval "$key=\"$val\"" ;;
        esac
    done < "$file"

    [ -n "$name" ] && [ -n "$version" ]
}

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
    
    read_recipe ./kpkgbuild || {
        echo "ERROR: Failed to read kpkgbuild for $port" >&2
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

get_port_version() {
    local port=$1
    local portdir="$WORKSPACE/ports/core/$port"
    
    if [ ! -d "$portdir" ]; then
        echo "ERROR: Port not found: $port" >&2
        return 1
    fi
    
    read_recipe "$portdir/kpkgbuild" || {
        echo "ERROR: Failed to read kpkgbuild for $port" >&2
        return 1
    }
    
    echo "$version"
}
