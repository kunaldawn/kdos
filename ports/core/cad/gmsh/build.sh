# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# The FLTK GUI with OpenCASCADE for STEP/IGES/BREP, and the shared library
# with its Python API, which FreeCAD's FEM workbench drives. MED, CGNS, Mmg
# and PETSc have no port, so each is off rather than probed and dropped.
cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DENABLE_BUILD_SHARED=ON \
	-DENABLE_BUILD_DYNAMIC=ON \
	-DENABLE_FLTK=ON \
	-DENABLE_CAIRO=ON \
	-DENABLE_OCC=ON \
	-DENABLE_OCC_CAF=ON \
	-DENABLE_EIGEN=ON \
	-DENABLE_GMP=ON \
	-DENABLE_OPENMP=ON \
	-DENABLE_MED=OFF \
	-DENABLE_CGNS=OFF \
	-DENABLE_MMG=OFF \
	-DENABLE_PETSC=OFF \
	-DENABLE_SLEPC=OFF \
	-DENABLE_MUMPS=OFF \
	-DENABLE_MPI=OFF \
	-DENABLE_POPPLER=OFF \
	-DENABLE_OSMESA=OFF \
	-DENABLE_TESTS=OFF \
	-DENABLE_RPATH=OFF \
	-DENABLE_PACKAGE_STRIP=OFF
cmake --build build
DESTDIR=$PKG cmake --install build

# gmsh.py finds libgmsh by walking up from its own directory, so from
# site-packages it reaches /usr/lib; left in /usr/lib no import finds it.
_site=$(python3 -c 'import sysconfig; print(sysconfig.get_path("purelib"))')
install -d "$PKG$_site"
mv "$PKG/usr/lib/gmsh.py" "$PKG$_site/gmsh.py"
rm -f "$PKG/usr/lib/gmsh.jl"

install -Dm644 utils/icons/gmsh.svg "$PKG/usr/share/icons/hicolor/scalable/apps/gmsh.svg"
install -d "$PKG/usr/share/icons/hicolor/256x256/apps"
rsvg-convert -w 256 -h 256 utils/icons/gmsh.svg \
	-o "$PKG/usr/share/icons/hicolor/256x256/apps/gmsh.png"
install -Dm644 utils/freedesktop/info.gmsh.gmsh.metainfo.xml \
	"$PKG/usr/share/metainfo/info.gmsh.gmsh.metainfo.xml"

# FLTK takes the window's app_id from argv[0].
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/info.gmsh.gmsh.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Gmsh
GenericName=Mesh Generator
Comment=Three-dimensional finite element mesh generator with pre- and post-processing
Exec=gmsh %F
Icon=gmsh
Terminal=false
StartupWMClass=gmsh
Categories=Science;Engineering;NumericalAnalysis;
Keywords=mesh;finite;element;fem;cad;step;geo;
DESKTOP
chmod 644 "$PKG/usr/share/applications/info.gmsh.gmsh.desktop"
