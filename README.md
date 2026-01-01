# KDOS : KD's Homebrew Linux Distro
```
     .--.
    |o_o |
    |:_/ |
   //   \ \
  (| KDOS |)
 /'\_   _/`\
 \___)=(___/
 ```

> "In the beginning, there was `void main()`. And then came KDOS."

**KDOS** is a handmade, artisan-crafted Linux distribution built from the absolute ground up, following the sacred texts of **Linux From Scratch (LFS)**. It is not for the faint of heart, the bloat-lovers, or those who fear compiler warnings. It is for the purists.

Powered by **Musl Libc** and **Toybox**, KDOS aims to be the digital equivalent of a katana: minimal, sharp, and lightweight.

---

## The Philosophy

Most distros give you a house. KDOS gives you a pile of bricks, a trowel, and a blueprint written in Bash.

1.  **LFS Roots**: We believe in understanding every single byte that goes into the OS. If we didn't compile it, we don't trust it.
2.  **Musl Libc**: Glibc is a metropolitan city. Musl is a Zen garden. We choose the garden. Static linking is our religion.
3.  **Toybox**: Why have 100 core utilities when you can have *one* statically linked binary that rules them all?
4.  **No SystemD**: We init like our ancestors did. Simple serial execution.

---

## The Architecture of Creation

The creation of KDOS is a three-act play, performed by your CPU cores.

### Act I: The Genesis (Toolchain)
*Script Location: `script/01_toolchain`*

Before we can build the world, we must build the tools to build the world. The host system's compiler is impure; it is tainted by the host's libraries and configuration.
*   **Binutils & GCC**: We forge a **Cross-Compiler**. This is a compiler that runs on your current distro but outputs code for KDOS. It is the bridge between the old world and the new.

### Act II: The Bootstrap (Phase 1)
*Script Location: `script/02_phase1`*

Using our pristine Cross-Compiler, we construct a temporary system in `build/fs`.
*   **Musl Libc**: The heart is transplanted first.
*   **Linux Headers**: The nervous system, defining the syscalls.
*   **Toybox & Bash**: Basic limbs to move around.
*   **KPKG**: Our homegrown package manager (because `apt` is too mainstream).
This phase creates a minimalist environment that is just capable enough to host a build process. It is the scaffolding.

### Act III: The Ascendance (Phase 2)
*Script Location: `script/03_phase2`*

This is the ritual of **Chroot**. We seal ourselves inside `build/fs`, cutting off all contact with the host OS. We are now truly inside KDOS.
*   We rebuild everything again, but this time, *from within*.
*   The result is a pure system, self-hosted and self-aware.

---

## Step-by-Step Build Process

### Prerequisites
You need a Linux host with:
*   `build-essential` (Make, GCC, etc.)
*   `python3` (For our beautiful TUI)
*   `bison`, `flex`, `texinfo` (The usual suspects)

### The Command

You have two paths to enlightenment:

#### 1. The Native Path
For the brave who trust their host environment.

```bash
./script/build.sh
```

#### 2. The Makefile Shortcut
If you just want to see the world burn (and build):

```bash
make       # Defaults to 'make build'
make run   # Boots the ISO in QEMU (if you have qemu-system-x86_64)
make clean # Destroys the evidence
```

---

## Fully Offline Ready

KDOS is designed for the apocalypse.
*   **Self-Contained Ports**: The `ports/` directory isn't just a list of URLs. It contains the logic to fetch, cache, and build everything.
*   **Offline Capability**: Run `make fetch` once to download all source tarballs. after that, you can pull the ethernet cable. The build process will happily continue without internet access, using cached sources.
*   **Zero External Dependencies**: Once fetched, the `src` and `ports` directories contain every single byte required to generate the OS.


### What Happens Next?

1.  **TUI Initialization**: If Python3 is detected, `script/build_tui.py` launches a modern, curses-based interface to visualize the suffering of your CPU.
2.  **Discovery**: The system dynamically scans `script/` for phases (folders like `01`, `02`...).
3.  **Compilation**:
    *   **Phase 1**: Scripts run on your host. Watch as `gcc` iterates passes.
    *   **Phase 2**: The runner detects `phase2.env.sh` has `CHROOT=1` and automatically wraps execution in `script/chroot_exec.sh`.
4.  **Completion**: When the progress bar hits 100%, `build/fs` contains your new OS root.

---

## Packet Management: KPKG

We don't use `.deb` or `.rpm`. We use `.kpkg` — because we invented it five minutes ago.
*   `kpkgbuild`: The architect. Reads a recipe and compiles source.
*   `kpkg`: The installer. Copies binaries to their final resting place.
*   Located in `src/kpkg/`.

### How to Add a New Port

Want to add `vim`? `htop`? `doom`? Here is how you can contribute to the chaos.

#### 1. Create the Directory
Inside `ports/core/` (or a category of your choice), create a folder for your package.
```bash
mkdir -p ports/core/mypackage
```

#### 2. The Sacred Scroll (`kpkgbuild`)
Create a `kpkgbuild` file inside that directory. It requires a few variables and a `build()` function.

```bash
# description : A useful one-liner about the package
# depends     : libfoo bar (space separated dependencies)

name=mypackage
version=1.0.0
release=1
source="https://example.com/downloads/$name-$version.tar.gz"

build() {
    # 1. Enter the source directory (kpkg handles extraction)
    cd $name-$version

    # 2. Configure (if needed)
    ./configure --prefix=/usr

    # 3. Compile
    make

    # 4. Install to the staging area ($PKG)
    # IMPORTANT: Do not install to / directly!
    make DESTDIR=$PKG install
}
```

#### 3. Fetch
Run `make fetch` to download the source tarball.

#### 4. Register & Build
The build system will automatically discover the new port if you add it to the phase script (e.g., `script/03_phase3/99_mypackage.sh`), OR you can build it manually inside the chroot environment using `kpkg`.

---

## License

MIT. Go forth and segfault.