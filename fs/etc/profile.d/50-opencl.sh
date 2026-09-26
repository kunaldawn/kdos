# Which GPUs Mesa's OpenCL driver offers.
#
# RUSTICL IS THE ONLY OPENCL DRIVER ON THIS IMAGE, AND IT OFFERS NOTHING UNTIL
# TOLD WHICH GALLIUM DRIVERS TO EXPOSE. ocl-icd finds its ICD in
# /etc/OpenCL/vendors either way; without this list clGetDeviceIDs answers "no
# devices" on Intel and AMD hardware, and every OpenCL program on the host
# (john's *-opencl formats, ffmpeg's *_opencl filters) finds no device.
# Mesa's build option for a default list accepts only radeonsi of the drivers
# this image's GPUs use, so the list is set here at run time: iris for Intel
# Gen8 and later, radeonsi for AMD GCN and later. llvmpipe is left out: a
# program that takes the first device it is offered could land on the CPU on a
# machine that has a GPU, and report that OpenCL works.
#
# NOT SET IF THE PERSON ALREADY SET IT: `RUSTICL_ENABLE=llvmpipe` or an empty
# value is a choice, and it outranks the distribution's.
[ -n "${RUSTICL_ENABLE+set}" ] || export RUSTICL_ENABLE=iris,radeonsi
