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
TMP_DIR=${ROOT_DIR}/tmp

get_latest_kernel_version() {
	if command -v jq >/dev/null; then
		curl -s https://www.kernel.org/releases.json | jq -r '.latest_stable.version'
	else
		curl -s https://www.kernel.org | grep -A1 'latest_link' | grep -oP 'linux-\K[0-9]+\.[0-9]+\.[0-9]+'
	fi
}

install_build_deps() {
	local packages=(build-essential libncurses-dev libssl-dev libelf-dev bison flex pahole bc busybox-static cpio qemu-system-x86 gdb)
	local missing=()
	for pkg in "${packages[@]}"; do
		if ! dpkg -l "$pkg" >/dev/null 2>&1; then
			missing+=("$pkg")
		fi
	done
	if [ ${#missing[@]} -gt 0 ]; then
		logwarn "Installing missing packages: ${missing[*]}"
		sudo apt-get update && sudo apt-get install -y "${missing[@]}"
	else
		logok "All build dependencies are installed."
	fi
}

build_busybox() {
	# busybox-static also can build by self
	local LATEST_VER="1.37.0"
	local TAR_NAME="busybox-${LATEST_VER}.tar.bz2"
	local SIGN_NAME="${TAR_NAME}.sig"
	local MAJOR_VER=$(echo "${LATEST_VER}" | cut -d. -f1)
	local URL="https://busybox.net/downloads/"
	local LOCAL_TAR="${ROOT_DIR}/${TAR_NAME}"
	local LOCAL_SIGN="${ROOT_DIR}/${SIGN_NAME}"
	local SOURCE_DIR="${ROOT_DIR}/busybox-${LATEST_VER}"

	if [[ ! -f "${LOCAL_TAR}" ]]; then
		# clean
		loginfo "Cleaning directory (keeping ${SCRIPT_NAME})..."
		rm -rf ${ROOT_DIR}/busybox-*.tar*
		rm -rf ${ROOT_DIR}/busybox-*/

		# download
		loginfo "Downloading ${TAR_NAME} ..."
		wget -O "${LOCAL_TAR}" "${URL}/${TAR_NAME}"
		wget -O "${LOCAL_SIGN}" "${URL}/${SIGN_NAME}"
	else
		logok "busybox tarball already exists: ${TAR_NAME}"
	fi

	if [[ ! -d "${SOURCE_DIR}" ]]; then
		loginfo "Extracting ${TAR_NAME} ..."
		tar -xf "${LOCAL_TAR}" -C "${ROOT_DIR}"
	else
		logok "Source directory already exists: ${SOURCE_DIR}"
	fi

	install_build_deps
	loginfo "Building busybox: ${LATEST_VER}"
	cd "${SOURCE_DIR}"
	make clean
	make defconfig
	# 1. open Static bin
	# Settings -> Build static binary (no shared libs)
	# 2. close all log
	# System Logging Utilities -> close all
	# 3. close tc
	# Netowrking Utilities -> tc
	# Save -> Exit
	make menuconfig
	make -j $(nproc)
	rm -rf ${TMP_DIR}/_install
	make CONFIG_PREFIX="${TMP_DIR}/_install" install # default CONFIG_PREFIX="${SOURCE_DIR}/_install"

	cd ${TMP_DIR}/_install
	mkdir -p dev proc sys
	cat <<EOF >init
#!/bin/sh
mount -t devtmpfs none /dev
mount -t proc     none /proc
mount -t sysfs    none /sys

clear
echo "Wecome to CPUNCHLINE!"

exec /bin/sh
EOF

	chmod +x init
	rm -rf ${TMP_DIR}/cpunchline_fs.gz
	find . -print0 | cpio -ov --format=newc -0 | gzip -9 >${TMP_DIR}/cpunchline_fs.gz
}

run_qemu_debug() {
	local SOURCE_DIR="$1"
	loginfo "$(hexdump -n 512 -vC ${SOURCE_DIR}/arch/x86/boot/bzImage)"
    # start location : 4d 5a
    # end location   : 55 aa
	qemu-system-x86_64 \
		-kernel ${SOURCE_DIR}/arch/x86/boot/bzImage \
		-initrd "${TMP_DIR}/cpunchline_fs.gz" \
		-append "root=/dev/ram rw console=ttyS0 nokaslr" \
		-nographic \
		-s \
		-S
}

build_kernel() {
	local LATEST_VER
	LATEST_VER=$(get_latest_kernel_version)
	[[ -z "${LATEST_VER}" ]] && {
		logerr "Failed to get latest kernel version"
		exit 1
	}
	loginfo "Latest kernel version: ${LATEST_VER}"
	local TAR_NAME="linux-${LATEST_VER}.tar.xz"
	local SIGN_NAME="linux-${LATEST_VER}.tar.sign"
	local MAJOR_VER=$(echo "${LATEST_VER}" | cut -d. -f1)
	local URL="https://cdn.kernel.org/pub/linux/kernel/v${MAJOR_VER}.x"
	local LOCAL_TAR="${ROOT_DIR}/${TAR_NAME}"
	local LOCAL_SIGN="${ROOT_DIR}/${SIGN_NAME}"
	local SOURCE_DIR="${ROOT_DIR}/linux-${LATEST_VER}"

	if [[ ! -f "${LOCAL_TAR}" ]]; then
		# clean
		loginfo "Cleaning directory (keeping ${SCRIPT_NAME})..."
		rm -rf ${ROOT_DIR}/linux-*.tar*
		rm -rf ${ROOT_DIR}/linux-*/

		# download
		loginfo "Downloading ${TAR_NAME} ..."
		wget -O "${LOCAL_TAR}" "${URL}/${TAR_NAME}"
		wget -O "${LOCAL_SIGN}" "${URL}/${SIGN_NAME}"
	else
		logok "Kernel tarball already exists: ${TAR_NAME}"
	fi

	if [[ ! -d "${SOURCE_DIR}" ]]; then
		loginfo "Extracting ${TAR_NAME} ..."
		tar -xf "${LOCAL_TAR}" -C "${ROOT_DIR}"
	else
		logok "Source directory already exists: ${SOURCE_DIR}"
	fi

	install_build_deps
	loginfo "Building kernel: ${LATEST_VER}"
	cd "${SOURCE_DIR}"
	make clean
	make defconfig
	# open Debug
	# Kernel hacking -> Compile-time checks and compiler options
	# -> Debug information (Disable debug information)
	# -> Rely on the toolchain's implicit default DWARF version
	# -> Save -> Exit
	make menuconfig
	make -j $(nproc) LOCALVERSION="-CPUNCHLINE"
	# make modules_install                        # modules install
	# make install                                # mirror install
	# sudo grub-mkconfig -o /boot/grub/grub.cfg   # upgrade startup menu

	run_qemu_debug "${SOURCE_DIR}"

	# another terminal
	# cd ${SOURCE_DIR}
	# gdb ./vmlinux
	# (gdb) target remote :1234
	# (gdb) b *0x100000
	# (gdb) b start_kernel
	# (gdb) c
	# (gdb) c
	# (gdb) l
}

main() {
	case "$1" in
	kernel)
		build_kernel
		;;
	busybox)
		build_busybox
		;;
	*)
		logerr "Invalid argument."
		exit 1
		;;
	esac
}

if [ "$#" -ne 1 ]; then
	logerr "Usage: $0 {kernel|busybox}"
	exit 1
fi

if [ -d "${TMP_DIR}" ]; then
	rm -rf "${TMP_DIR}/*"
else
	mkdir -p "${TMP_DIR}"
fi

main "$1"
