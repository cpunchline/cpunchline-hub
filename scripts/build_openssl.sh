#!/bin/bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")" && pwd)
SOURCE_DIR=$(dirname "${ROOT_DIR}")
PRE_BUILT_DIR=${SOURCE_DIR}/tools/pre_built

if [ $# -eq 0 ]; then
	echo "please input the openssl source dir"
	exit 1
fi

OPENSSL_SOURCE_DIR=$1
OPENSSL_CONFIGURE_OPTS=(
	linux-x86_64 # ARM32: linux-armv4; ARM64: linux-aarch64
	no-asm
	no-tests
	no-docs
	no-ec2m
	disable-devcryptoeng
	# --debug
	# no-deprecated
	# no-legacy
	# --cross-compile-prefix=xxx
)

# 	-Wa,--noexecstack -fstack-protector-strong -fomit-frame-pointer -fexpensive-optimizations -frename-registers -ftree-vectorize \
#	-Wformat=2 -Wformat-security -D_FORTIFY_SOURCE=2 \
#	-pie -Wl,--hash-style=gnu -Wl,--as-needed -Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now \
#	-Wl,-rpath=\$ORIGIN/lib64:\$ORIGIN/../lib64:\$ORIGIN/lib:\$ORIGIN/../lib

cd ${OPENSSL_SOURCE_DIR}

./config "${OPENSSL_CONFIGURE_OPTS[@]}" \
	--prefix="${PRE_BUILT_DIR}" \
	--libdir=lib \
	--openssldir="${PRE_BUILT_DIR}/ssl"
# \
# --cross-compile-prefix=${TOOLCHAIN_DIR} \

make clean
make -j$(nproc)
make install_sw

# patchelf --set-rpath '$ORIGIN/lib64:$ORIGIN/../lib64:$ORIGIN/lib:$ORIGIN/../lib' ${PRE_BUILT_DIR}/bin/openssl

# openssl命令行: ${PRE_BUILT_DIR}/bin
# openssl配置: ${PRE_BUILT_DIR}/ssl
# openssl头文件: ${PRE_BUILT_DIR}/include
# openssl库文件: ${PRE_BUILT_DIR}/lib