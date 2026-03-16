#!/bin/bash
set -euo pipefail

# sudo apt install gcc-aarch64-linux-gnu gcc-arm-linux-gnueabihf
# source ./environment-setup.sh

# Check for LD_LIBRARY_PATH being set, which can break SDK and generally is a bad practice
# Only disable this check if you are absolutely know what you are doing!
if printenv LD_LIBRARY_PATH >/dev/null; then
	echo "LD_LIBRARY_PATH = ${LD_LIBRARY_PATH}"
	echo "Your environment is misconfigured, you probably need to 'unset LD_LIBRARY_PATH'"
	echo "but please check why this was set in the first place and that it's safe to unset."
	echo "The SDK will not operate correctly in most cases when LD_LIBRARY_PATH is set."
	return 1
fi

unset SYSROOT_DIR
unset CROSS_DIR
unset CROSS_PREFIX
unset CC
unset CPP
unset CXX
unset AS
unset LD
unset STRIP
unset RANLIB
unset OBJCOPY
unset OBJDUMP
unset AR
unset NM
unset READELF
unset CFLAGS
unset CPPFLAGS
unset CXXFLAGS
unset LDFLAGS

export SYSROOT_DIR="/"
export CROSS_DIR="/usr/bin"
export CROSS_PREFIX="" # arm-linux-gnueabihf-
export CC="${CROSS_DIR}/${CROSS_PREFIX}gcc"
export CPP="${CROSS_DIR}/${CROSS_PREFIX}g++"
export CXX="${CROSS_DIR}/${CROSS_PREFIX}g++"
export AS="${CROSS_DIR}/${CROSS_PREFIX}as"
export LD="${CROSS_DIR}/${CROSS_PREFIX}ld"
export STRIP="${CROSS_DIR}/${CROSS_PREFIX}strip"
export RANLIB="${CROSS_DIR}/${CROSS_PREFIX}ranlib"
export OBJCOPY="${CROSS_DIR}/${CROSS_PREFIX}objcopy"
export OBJDUMP="${CROSS_DIR}/${CROSS_PREFIX}objdump"
export AR="${CROSS_DIR}/${CROSS_PREFIX}ar"
export NM="${CROSS_DIR}/${CROSS_PREFIX}nm"
export READELF="${CROSS_DIR}/${CROSS_PREFIX}readelf"
export CFLAGS="--sysroot=${SYSROOT_DIR}"
export CPPFLAGS="--sysroot=${SYSROOT_DIR}"
export CXXFLAGS="--sysroot=${SYSROOT_DIR}"
export LDFLAGS="--sysroot=${SYSROOT_DIR} -Wl,--as-needed -Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now"

HOST_OS=$(uname -s)
HOST_ARCH=$(uname -m)
TARGET_PLATFORM=$(${CC} -v 2>&1 | grep Target | sed 's/Target: //')
TARGET_ARCH=$(echo ${TARGET_PLATFORM} | awk -F'-' '{print $1}')
# TARGET_OS,TARGET_ARCH used by make
case ${TARGET_PLATFORM} in
*mingw*) TARGET_OS=Windows ;;
*android*) TARGET_OS=Android ;;
*darwin*) TARGET_OS=Darwin ;;
*) TARGET_OS=Linux ;;
esac

echo "CC          =   ${CC}"
echo "CPP         =   ${CPP}"
echo "CXX         =   ${CXX}"
if [ ${CC} ]; then
	CC_VERSION=$(${CC} --version 2>&1 | head -n 1)
	echo "CC_VERSION  =   ${CC_VERSION}"
fi
echo "AS          =   ${AS}"
echo "LD          =   ${LD}"
echo "STRIP       =   ${STRIP}"
echo "RANLIB      =   ${RANLIB}"
echo "OBJCOPY     =   ${OBJCOPY}"
echo "OBJDUMP     =   ${OBJDUMP}"
echo "AR          =   ${AR}"
echo "NM          =   ${NM}"
echo "READELF     =   ${READELF}"
echo "CFLAGS      =   ${CFLAGS}"
echo "CPPFLAGS    =   ${CPPFLAGS}"
echo "CXXFLAGS    =   ${CXXFLAGS}"
echo "LDFLAGS     =   ${LDFLAGS}"

echo "HOST_OS     =    ${HOST_OS}"
echo "HOST_ARCH   =    ${HOST_ARCH}"
echo "TARGET_OS   =    ${TARGET_OS}"
echo "TARGET_ARCH =    ${TARGET_ARCH}"
