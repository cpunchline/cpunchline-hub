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
SCRIPTS_DIR=${ROOT_DIR}/scripts
OUTPUT_DIR=${ROOT_DIR}/output
BUILD_DIR=${ROOT_DIR}/build

usage() {
	logwarn "Usage: $0 [options]"
	logwarn "    -d|--debug          build debug."
	logwarn "    -r|--release        build release."
	logwarn "    -c|--check          cppcheck and codeline."
	logwarn "    -3rd|--thirdparty   build thirdparty."
	logwarn "    -tc|--coverage      build tests and coverage."
	logwarn "    -h|--help           show help info."
}

INPUT_CMAKE_BUILD_TYPE=""
INPUT_CMAKE_CHECK=""
INPUT_CMAKE_TARGET=""
INPUT_CMAKE_BUILD_THIRDPARTY=""
INPUT_CMAKE_TESTS=""
INPUT_CMAKE_CONVERAGE=""

while (($#)); do
	case $1 in
	-d | --debug)
		[ -n "${INPUT_CMAKE_BUILD_TYPE}" ] && {
			logerr "$1 mutually exclusive"
			exit 1
		}
		INPUT_CMAKE_BUILD_TYPE="Debug"
		INPUT_CMAKE_TARGET="install"
		shift 1
		;;
	-r | --release)
		[ -n "${INPUT_CMAKE_BUILD_TYPE}" ] && {
			logerr "$1 mutually exclusive"
			exit 1
		}
		INPUT_CMAKE_BUILD_TYPE="Release"
		INPUT_CMAKE_TARGET="install/strip"
		shift 1
		;;
	-c | --check)
		INPUT_CMAKE_CHECK="ON"
		shift 1
		;;
	-3rd | --thirdparty)
		INPUT_CMAKE_BUILD_THIRDPARTY="ON"
		shift 1
		;;
	-tc | --coverage)
		INPUT_CMAKE_TESTS="ON"
		INPUT_CMAKE_CONVERAGE="ON"
		shift 1
		;;
	-h | --help)
		usage
		exit 0
		;;
	*)
		logerr "invalid param: $1"
		exit 1
		;;
	esac
done

: "${INPUT_CMAKE_BUILD_TYPE:=Debug}"
: "${INPUT_CMAKE_TARGET:=all}"
: "${INPUT_CMAKE_BUILD_THIRDPARTY:=OFF}"
: "${INPUT_CMAKE_CHECK:=OFF}"
: "${INPUT_CMAKE_TESTS:=OFF}"
: "${INPUT_CMAKE_CONVERAGE:=OFF}"

logwarn "-- INPUT_CMAKE_BUILD_TYPE=${INPUT_CMAKE_BUILD_TYPE}"
logwarn "-- INPUT_CMAKE_TARGET=${INPUT_CMAKE_TARGET}"
logwarn "-- INPUT_CMAKE_CHECK=${INPUT_CMAKE_CHECK}"
logwarn "-- INPUT_CMAKE_BUILD_THIRDPARTY=${INPUT_CMAKE_BUILD_THIRDPARTY}"
logwarn "-- INPUT_CMAKE_TESTS=${INPUT_CMAKE_TESTS}"
logwarn "-- INPUT_CMAKE_CONVERAGE=${INPUT_CMAKE_CONVERAGE}"

export CCACHE_DIR=${ROOT_DIR}/.cache # ccache cache

# ${SCRIPTS_DIR}/environment-setup.sh
cmake \
	--fresh \
	-S "${ROOT_DIR}" \
	-B "${BUILD_DIR}" \
	-G "Ninja" \
	-DCMAKE_BUILD_TYPE="${INPUT_CMAKE_BUILD_TYPE}" \
	-DCPUNCHLINE_BUILD_THIRDPARTY="${INPUT_CMAKE_BUILD_THIRDPARTY}" \
	-DCPUNCHLINE_BUILD_TESTS="${INPUT_CMAKE_TESTS}" \
	-DCPUNCHLINE_BUILD_COVERAGE="${INPUT_CMAKE_CONVERAGE}"
[ $? -eq 0 ] || {
	logerr "cmake config fail!"
	exit 1
}

cmake \
	--build "${BUILD_DIR}" \
	--target "${INPUT_CMAKE_TARGET}" \
	--parallel $(nproc)
# 2> >(cat >&2) | grep -v "Up-to-date"
[ $? -eq 0 ] || {
	logerr "cmake build fail!"
	exit 1
}

if [ "${INPUT_CMAKE_CHECK}" == "ON" ]; then
	${SCRIPTS_DIR}/codeline.sh
	[ $? -eq 0 ] || {
		logerr "codeline report fail!"
		exit 1
	}
	${SCRIPTS_DIR}/cppcheck_report.sh
	[ $? -eq 0 ] || {
		logerr "cppcheck report fail!"
		exit 1
	}
fi

# export PKG_CONFIG_PATH="${ROOT_DIR}/tools/pre_built/lib/pkgconfig:$PKG_CONFIG_PATH"

# --verbose
# --target install/strip
# --install-prefix

# cmake --graphviz=build.dot ${ROOT_DIR}
# sudo apt install graphviz
# dot -Tpng -o build.png build.dot
# dot -Tpdf -o build.pdf build.dot

# cmake --build ${BUILD_DIR} --target clang-tidy
# ./scripts/cppcheck_report.sh ${ROOT_DIR}
