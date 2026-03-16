#!/bin/bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")" && pwd)
SOURCE_DIR=$(dirname "${ROOT_DIR}")
CODELINE_OUTPUT_DIR=${SOURCE_DIR}/output/codeline_report
CODELINE_BYFILE_MD=${CODELINE_OUTPUT_DIR}/codeline_byfile.md
CODELINE_BYLANGUAGE_MD=${CODELINE_OUTPUT_DIR}/codeline_bylanguage.md

command -v cloc >/dev/null 2>&1 || {
	echo >&2 "cloc is not installed but required. Trying to install it..."
	sudo apt-get -y install cloc
}

cd ${SOURCE_DIR}
cloc \
	--vcs=git \
	--by-file \
	--fullpath --not-match-d=tools/pre_built \
	-md \
	--report-file=${CODELINE_BYFILE_MD}

cloc \
	--vcs=git \
	--by-percent=c \
	--fullpath --not-match-d=tools/pre_built \
	-md \
	--report-file=${CODELINE_BYLANGUAGE_MD}
