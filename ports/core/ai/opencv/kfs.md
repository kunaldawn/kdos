## About

OpenCV is the computer-vision library: image processing, feature detection, camera calibration,
object tracking, video input and output, and a neural-network inference module, with Python
bindings (`cv2`). The port is the 4.x line, the API [digikam](#port-digikam), [mlt](#port-mlt)
(the motion tracker of Kdenlive and Shotcut), [sdrangel](#port-sdrangel),
[wl-kbptr](#port-wl-kbptr) and the other consumers are written against. Eight modules come from
the separate `opencv_contrib` tarball.

## Flags

- `-DOPENCV_EXTRA_MODULES_PATH="$SRC/contrib"` — Configure the contrib modules from `contrib/`,
  into which `build.sh` moves only `aruco`, `tracking`, `optflow`, `plot`, `shape`, `superres`,
  `videostab` and `ximgproc`, so the rest of `opencv_contrib` is never configured. Pointing the
  path at the whole `modules/` directory would configure every contrib module, `face` among them,
  whose configure downloads a landmark model.
- `-DCPU_BASELINE=SSE2 -DCPU_BASELINE_DISABLE=SSE3` — Compile the whole library for the x86-64
  baseline instructions only. Upstream's default baseline on x86-64 is SSE3, which a program would
  then require of the processor; the faster SSE4, AVX, AVX2 and AVX-512 kernels stay in the
  dispatch list and are chosen at run time on a processor that has them.
- `-DCMAKE_SKIP_INSTALL_RPATH=ON` — Install the libraries and programs without a run path; they
  are in `/usr/lib`.
- `-DOPENCV_GENERATE_PKGCONFIG=ON` — Install `opencv4.pc`, through which meson and autotools
  consumers, such as [wl-kbptr](#port-wl-kbptr), find OpenCV. Upstream leaves it off.
- `-DOPENCV_GENERATE_SETUPVARS=OFF` — Do not install `setup_vars_opencv4.sh`, a shell script
  that sets the environment variables for using the install; with everything under `/usr`, none
  is needed.
- `-DOPENCV_ENABLE_NONFREE=OFF` — Leave out the algorithms upstream marks non-free (patented).
- `-DBUILD_opencv_apps=ON` — Install the `opencv_*` command-line tools (`opencv_version`,
  `opencv_annotation`, `opencv_interactive-calibration` and others).
- `-DBUILD_opencv_gapi=OFF`, `-DWITH_ADE=OFF` — The G-API graph module, which needs the ADE
  library that OpenCV downloads at configure time.
- `-DBUILD_opencv_python3=ON`, `-DPYTHON3_EXECUTABLE=/usr/bin/python3` — Build the `cv2` module
  for the `python3` port, against [numpy](#port-numpy).
- `-DOPENCV_SKIP_PYTHON_LOADER=ON`, `-DOPENCV_PYTHON3_INSTALL_PATH="$_pydir"` — Install the
  compiled `cv2` extension straight into `site-packages` (`$_pydir`, read from Python's own
  `sysconfig`), instead of OpenCV's loader package, a `cv2/` directory of Python files that finds
  the binary through a generated configuration.
- `-DBUILD_opencv_python2=OFF`, `-DBUILD_JAVA=OFF`, `-DBUILD_opencv_js=OFF` — No Python 2, Java or
  JavaScript bindings.
- `-DBUILD_ZLIB=OFF`, `-DBUILD_JPEG=OFF`, `-DBUILD_PNG=OFF`, `-DBUILD_TIFF=OFF`,
  `-DBUILD_WEBP=OFF`, `-DBUILD_OPENJPEG=OFF`, `-DBUILD_OPENEXR=OFF`, `-DBUILD_JASPER=OFF`,
  `-DBUILD_TBB=OFF` — Never compile the copies under `3rdparty/`; every codec that is enabled, and
  TBB, comes from its port in `depends`.
- `-DBUILD_IPP_IW=OFF`, `-DWITH_IPP=OFF`, `-DBUILD_ITT=OFF`, `-DWITH_ITT=OFF` — Intel's IPP
  primitives, a prebuilt binary OpenCV downloads at configure, and the ITT tracing hooks.
- `-DBUILD_PROTOBUF=ON`, `-DPROTOBUF_UPDATE_FILES=OFF`, `-DWITH_PROTOBUF=ON` — The DNN module's
  model importers use the protobuf runtime OpenCV carries in `3rdparty/protobuf`, linked into
  `libopencv_dnn`, together with the `.pb.cc` files upstream generated against that copy.
  Building against the [protobuf](#port-protobuf) port instead would need the generated files
  regenerated with its `protoc` (`PROTOBUF_UPDATE_FILES=ON`), which upstream's build refuses to
  combine with the bundled runtime.
- `-DWITH_FLATBUFFERS=ON` — The TFLite importer of the DNN module, through the header-only
  FlatBuffers copy in `3rdparty/`; FlatBuffers is not a port.
- `-DWITH_QUIRC=ON` — QR-code decoding through the quirc copy in `3rdparty/`; quirc is not a port.
  Upstream leaves it off.
- `-DWITH_JPEG=ON`, `-DWITH_PNG=ON`, `-DWITH_TIFF=ON`, `-DWITH_WEBP=ON`, `-DWITH_OPENJPEG=ON`,
  `-DWITH_OPENEXR=ON`, `-DWITH_AVIF=ON`, `-DWITH_JPEGXL=ON` — Image codecs, each through its
  port.
- `-DWITH_SPNG=OFF`, `-DWITH_JASPER=OFF` — The second PNG and JPEG 2000 readers; PNG goes through
  libpng and JPEG 2000 through [openjpeg](#port-openjpeg).
- `-DWITH_GDAL=OFF`, `-DWITH_GDCM=OFF` — Geospatial rasters through GDAL and DICOM through GDCM.
  [gdal](#port-gdal) is a port but not in `depends`; GDCM is not a port.
- `-DWITH_FFMPEG=ON`, `-DWITH_GSTREAMER=ON`, `-DWITH_V4L=ON` — Video input and output through
  [ffmpeg](#port-ffmpeg) and [gstreamer](#port-gstreamer), and cameras through V4L2.
- `-DWITH_VA=ON`, `-DWITH_VA_INTEL=OFF` — VA-API hardware decoding through [libva](#port-libva),
  without the Intel-specific VA-API/OpenCL interoperability.
- `-DWITH_1394=OFF`, `-DWITH_GPHOTO2=OFF`, `-DWITH_OPENNI2=OFF`, `-DWITH_OBSENSOR=OFF`,
  `-DWITH_PVAPI=OFF`, `-DWITH_ARAVIS=OFF`, `-DWITH_XIMEA=OFF`, `-DWITH_UEYE=OFF` — The FireWire,
  gPhoto2, OpenNI 2, Orbbec, Prosilica, GenICam (Aravis), XIMEA and IDS uEye camera backends.
  On Linux the Orbbec backend needs only the V4L2 headers and is on by default; the libraries the
  others need are not ports, apart from [libgphoto2](#port-libgphoto2), which is not in
  `depends`.
- `-DWITH_QT=ON`, `-DWITH_GTK=OFF`, `-DWITH_WAYLAND=OFF`, `-DWITH_FRAMEBUFFER=OFF` — HighGUI, the
  `imshow` window, draws through [qt6-qtbase](#port-qt6-qtbase); the GTK backend, OpenCV's own
  Wayland backend and the framebuffer backend are left out.
- `-DWITH_OPENGL=OFF`, `-DWITH_VTK=OFF`, `-DWITH_VULKAN=OFF` — OpenGL windows in HighGUI, the
  `viz` module (VTK) and the Vulkan DNN backend.
- `-DWITH_OPENCL=ON` — The OpenCL acceleration (`cv::UMat`), which OpenCV loads at run time, so a
  machine with no OpenCL driver runs the CPU paths.
- `-DWITH_OPENCL_SVM=OFF`, `-DWITH_OPENCLAMDFFT=OFF`, `-DWITH_OPENCLAMDBLAS=OFF` — OpenCL shared
  virtual memory, and AMD's clFFT and clBLAS libraries, which are not ports.
- `-DWITH_CUDA=OFF` — NVIDIA CUDA, whose toolkit is not a port.
- `-DWITH_TBB=ON`, `-DWITH_OPENMP=OFF`, `-DWITH_PTHREADS_PF=ON` — Parallel loops run on
  [onetbb](#port-onetbb); OpenCV uses one parallel framework, and the pthreads one is its
  fallback.
- `-DWITH_EIGEN=ON`, `-DWITH_LAPACK=ON` — [eigen](#port-eigen) for some solvers, and LAPACK
  through [openblas](#port-openblas).
- `-DWITH_KLEIDICV=OFF`, `-DWITH_FASTCV=OFF` — Arm's KleidiCV, whose source OpenCV downloads at
  configure time, and Qualcomm's FastCV, a prebuilt library it downloads; both are Arm-only HAL
  backends.
- `-DWITH_HALIDE=OFF`, `-DWITH_OPENVINO=OFF`, `-DWITH_ONNX=OFF`, `-DWITH_TIMVX=OFF`,
  `-DWITH_CANN=OFF` — Further DNN backends: Halide, OpenVINO, ONNX Runtime, TIM-VX and Huawei
  CANN. [onnxruntime](#port-onnxruntime) is a port but not in `depends`; the rest are not ports.

## Patches

- `ffmpeg9.patch` — Upstream's change on the 4.x branch, after this release, to the videoio FFmpeg backend: where libavcodec is 61.13.100 or newer it asks for an encoder's pixel formats and frame rates with `avcodec_get_supported_config()`, instead of reading `AVCodec`'s `pix_fmts` and `supported_framerates` fields, which FFmpeg 9 does not have. The old code stays behind `#else` for an older FFmpeg.

## Notes

The two tarballs unpack side by side under `$SRC_ROOT`; `build.sh` runs in the OpenCV tree and
moves the eight contrib modules out of `opencv_contrib-<version>/modules/` into `contrib/`.
Both tarballs carry the same version, and a bump must move both `sha256` lines together, or the
contrib modules are built against a core they were not released with.

The `test` lines after the install stop the build unless the core and `tracking` libraries,
`opencv4.pc`, the headers and the `cv2` extension are all in the package, so a contrib module or
the Python binding that CMake quietly dropped cannot ship.
