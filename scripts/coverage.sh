#!/bin/bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")" && pwd)
SOURCE_DIR=$(dirname "${ROOT_DIR}")
BUILD_DIR=${SOURCE_DIR}/build
PROJECT_NAME=$(cat ${SOURCE_DIR}/CMakeLists.txt | grep "project(" | awk -F ' ' '{print $1}')
COVERAGE_REPORT_DIR=${SOURCE_DIR}/output/coverage

[ -d ${COVERAGE_REPORT_DIR} ] && {
	rm -rf ${COVERAGE_REPORT_DIR}
}
mkdir -p ${COVERAGE_REPORT_DIR}

command -v lcov >/dev/null 2>&1 || {
	echo >&2 "lcov is not installed but required. Trying to install it..."
	sudo apt-get -y install lcov
}

${SOURCE_DIR}/rebuild.sh -d -tc
lcov -b ${SOURCE_DIR} \
	--build-directory ${BUILD_DIR} \
	-d ${BUILD_DIR} \
	-z

${SOURCE_DIR}/build/tests/gtest_app

lcov -b ${SOURCE_DIR} \
	--build-directory ${BUILD_DIR} \
	-d ${BUILD_DIR} \
	-c \
	--no-external \
	--branch-coverage \
	--function-coverage \
	--checksum \
	--demangle-cpp \
	--ignore-errors mismatch \
	-o ${COVERAGE_REPORT_DIR}/coverage.info \
	-j $(nproc)
lcov --list ${COVERAGE_REPORT_DIR}/coverage.info

genhtml \
	-s \
	--source-directory ${SOURCE_DIR} \
	--branch-coverage \
	--function-coverage \
	--demangle-cpp \
	--dark-mode \
	-p ${SOURCE_DIR} \
	--title "${PROJECT_NAME}-$(date \+%F_%T)" \
	-o ${COVERAGE_REPORT_DIR} \
	${COVERAGE_REPORT_DIR}/coverage.info