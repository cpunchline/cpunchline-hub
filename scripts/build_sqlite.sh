#!/bin/bash
set -euo pipefail

# White
loginfo() {
	echo -e "\033[37m$1\033[0m"
}

# Yellow
logwarn() {
	echo -e "\033[33m$1\033[0m"
}

# Green
logok() {
	echo -e "\033[32m$1\033[0m"
}

# Red
logerr() {
	echo -e "\033[31m$1\033[0m"
}

ROOT_DIR=$(cd "$(dirname "$0")" && pwd)
SCRIPT_NAME=$(basename "$0")
PROJECT_DIR=$(dirname "${ROOT_DIR}")
PRE_BUILT_DIR=${PROJECT_DIR}/tools/pre_built
TMP_DIR=${ROOT_DIR}/tmp

build_sqlite() {
	local LATEST_VER=$(curl -s https://www.sqlite.org/download.html | awk -F, '/^PRODUCT,.*sqlite-autoconf-/{print $2; exit}')
	[[ -z "${LATEST_VER}" ]] && {
		logerr "Failed to get latest sqlite version"
		exit 1
	}

	local TARGZ_FILE=$(curl -s https://www.sqlite.org/download.html | awk -F, '/^PRODUCT,.*sqlite-autoconf-/{print $3; exit}')
	[[ -z "${TARGZ_FILE}" ]] && {
		logerr "Failed to get latest sqlite tarball"
		exit 1
	}
	TARGZ_BASENAME=${TARGZ_FILE##*/}
	TARGZ_BASENAME=${TARGZ_BASENAME%.tar.gz}

	local LOCAL_TAR="${ROOT_DIR}/sqlite-autoconf-${LATEST_VER}.tar.gz"

	logwarn "Latest sqlite version: ${LATEST_VER}"

	if [[ ! -f "${LOCAL_TAR}" ]]; then
		# clean
		loginfo "Cleaning directory (keeping ${SCRIPT_NAME})..."
		rm -rf ${ROOT_DIR}/sqlite-autoconf-*

		# download
		loginfo "Downloading ${LOCAL_TAR} ..."
		wget -O "${LOCAL_TAR}" "https://sqlite.org/${TARGZ_FILE}"
	else
		logok "sqlite tarball already exists: ${LOCAL_TAR}"
	fi

	local CONFIGURE_OPTS=(
		--disable-static
		--all
	)

	SOURCE_DIR=${TMP_DIR}/${TARGZ_BASENAME} # sqlite-autoconf-*

	if [[ ! -d "${SOURCE_DIR}" ]]; then
		loginfo "Extracting ${TARGZ_BASENAME} ..."
		tar -xf "${LOCAL_TAR}" -C "${TMP_DIR}"
	else
		logok "Source directory already exists: ${SOURCE_DIR}"
	fi

	# 	-Wa,--noexecstack -fstack-protector-strong -fomit-frame-pointer -fexpensive-optimizations -frename-registers -ftree-vectorize \
	#	-Wformat=2 -Wformat-security -D_FORTIFY_SOURCE=2 \
	#	-pie -Wl,--hash-style=gnu -Wl,--as-needed -Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now \
	#	-Wl,-rpath=\$ORIGIN/lib64:\$ORIGIN/../lib64:\$ORIGIN/lib:\$ORIGIN/../lib

	cd ${SOURCE_DIR}

	./configure "${CONFIGURE_OPTS[@]}" \
		--prefix="${PRE_BUILT_DIR}"

	make clean
	make -j$(nproc)
	make install
}

main() {
	build_sqlite
}

if [ -d "${TMP_DIR}" ]; then
	rm -rf "${TMP_DIR}/*"
else
	mkdir -p "${TMP_DIR}"
fi

main
