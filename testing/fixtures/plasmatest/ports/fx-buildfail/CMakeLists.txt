cmake_minimum_required(VERSION 3.22)
project(kdos-plasmatest-buildfail LANGUAGES CXX)

find_package(ECM 6.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(KDEInstallDirs)
include(KDECMakeSettings)

# No such package: the configure fails, and the build.sh line after the
# install succeeds, the shape that hides a failure when set -e is ignored.
find_package(KdosNoSuchPackage REQUIRED)
