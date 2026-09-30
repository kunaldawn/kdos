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

# The 4.x line: OpenCV 5 changed the API, and digiKam, MLT, SDRangel and
# the rest are written against 4.x.
#
# Contrib modules: only the ones this system's consumers use, moved into a
# directory of their own so the rest are never configured. aruco, tracking,
# optflow, plot, shape, superres, videostab and ximgproc serve MLT's motion
# tracker (Kdenlive, Shotcut) and digiKam. face is left out: its configure
# downloads a landmark model.
mkdir contrib
for m in aruco tracking optflow plot shape superres videostab ximgproc; do
	mv "$SRC_ROOT/opencv_contrib-$version/modules/$m" contrib/
done

# The dispatch baseline is the architecture's: SSE2 on x86_64 with the
# faster kernels chosen at run time, NEON on aarch64. KleidiCV and FastCV
# are prebuilt Arm libraries fetched at configure and are off; IPP and ADE
# (G-API) are likewise downloads and are off with G-API itself.
case "$(uname -m)" in
	x86_64) _cpu="-DCPU_BASELINE=SSE2 -DCPU_BASELINE_DISABLE=SSE3" ;;
	*) _cpu= ;;
esac

_pydir=$(python3 -c 'import sysconfig; print(sysconfig.get_path("platlib", "posix_prefix"))')

# HighGUI draws through Qt 6; GTK and OpenCV's own Wayland backend are off.
# Video I/O goes through FFmpeg, GStreamer and V4L2, with VA-API for decode.
# Every image codec uses its port, never the bundled copy. Protobuf is the
# bundled copy the DNN importers are generated against; OpenCL is loaded at
# run time. The viz module (VTK), OpenGL windows, Vulkan and the camera SDKs
# are off. Non-free algorithms stay off.
cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_SKIP_INSTALL_RPATH=ON \
	-DOPENCV_EXTRA_MODULES_PATH="$SRC/contrib" \
	-DOPENCV_GENERATE_PKGCONFIG=ON \
	-DOPENCV_GENERATE_SETUPVARS=OFF \
	-DOPENCV_ENABLE_NONFREE=OFF \
	$_cpu \
	-DBUILD_TESTS=OFF \
	-DBUILD_PERF_TESTS=OFF \
	-DBUILD_EXAMPLES=OFF \
	-DBUILD_DOCS=OFF \
	-DBUILD_PACKAGE=OFF \
	-DBUILD_opencv_apps=ON \
	-DBUILD_opencv_gapi=OFF \
	-DBUILD_opencv_python3=ON \
	-DBUILD_opencv_python2=OFF \
	-DBUILD_JAVA=OFF \
	-DBUILD_opencv_js=OFF \
	-DINSTALL_C_EXAMPLES=OFF \
	-DINSTALL_PYTHON_EXAMPLES=OFF \
	-DINSTALL_TESTS=OFF \
	-DPYTHON3_EXECUTABLE=/usr/bin/python3 \
	-DOPENCV_SKIP_PYTHON_LOADER=ON \
	-DOPENCV_PYTHON3_INSTALL_PATH="$_pydir" \
	-DBUILD_ZLIB=OFF \
	-DBUILD_JPEG=OFF \
	-DBUILD_PNG=OFF \
	-DBUILD_TIFF=OFF \
	-DBUILD_WEBP=OFF \
	-DBUILD_OPENJPEG=OFF \
	-DBUILD_OPENEXR=OFF \
	-DBUILD_JASPER=OFF \
	-DBUILD_TBB=OFF \
	-DBUILD_IPP_IW=OFF \
	-DBUILD_ITT=OFF \
	-DBUILD_PROTOBUF=ON \
	-DPROTOBUF_UPDATE_FILES=OFF \
	-DWITH_PROTOBUF=ON \
	-DWITH_FLATBUFFERS=ON \
	-DWITH_JPEG=ON \
	-DWITH_PNG=ON \
	-DWITH_SPNG=OFF \
	-DWITH_TIFF=ON \
	-DWITH_WEBP=ON \
	-DWITH_OPENJPEG=ON \
	-DWITH_JASPER=OFF \
	-DWITH_OPENEXR=ON \
	-DWITH_AVIF=ON \
	-DWITH_JPEGXL=ON \
	-DWITH_GDAL=OFF \
	-DWITH_GDCM=OFF \
	-DWITH_QUIRC=ON \
	-DWITH_FFMPEG=ON \
	-DWITH_GSTREAMER=ON \
	-DWITH_V4L=ON \
	-DWITH_VA=ON \
	-DWITH_VA_INTEL=OFF \
	-DWITH_1394=OFF \
	-DWITH_GPHOTO2=OFF \
	-DWITH_OPENNI2=OFF \
	-DWITH_OBSENSOR=OFF \
	-DWITH_PVAPI=OFF \
	-DWITH_ARAVIS=OFF \
	-DWITH_XIMEA=OFF \
	-DWITH_UEYE=OFF \
	-DWITH_QT=ON \
	-DWITH_GTK=OFF \
	-DWITH_WAYLAND=OFF \
	-DWITH_FRAMEBUFFER=OFF \
	-DWITH_OPENGL=OFF \
	-DWITH_VTK=OFF \
	-DWITH_VULKAN=OFF \
	-DWITH_OPENCL=ON \
	-DWITH_OPENCL_SVM=OFF \
	-DWITH_OPENCLAMDFFT=OFF \
	-DWITH_OPENCLAMDBLAS=OFF \
	-DWITH_CUDA=OFF \
	-DWITH_TBB=ON \
	-DWITH_OPENMP=OFF \
	-DWITH_PTHREADS_PF=ON \
	-DWITH_EIGEN=ON \
	-DWITH_LAPACK=ON \
	-DWITH_IPP=OFF \
	-DWITH_ITT=OFF \
	-DWITH_ADE=OFF \
	-DWITH_KLEIDICV=OFF \
	-DWITH_FASTCV=OFF \
	-DWITH_HALIDE=OFF \
	-DWITH_OPENVINO=OFF \
	-DWITH_ONNX=OFF \
	-DWITH_TIMVX=OFF \
	-DWITH_CANN=OFF
ninja -C build
DESTDIR=$PKG ninja -C build install

test -f "$PKG/usr/lib/libopencv_core.so"
test -f "$PKG/usr/lib/libopencv_tracking.so"
test -f "$PKG/usr/lib/pkgconfig/opencv4.pc"
test -f "$PKG/usr/include/opencv4/opencv2/core.hpp"
test -n "$(find "$PKG$_pydir" -name 'cv2*.so' | head -1)"
