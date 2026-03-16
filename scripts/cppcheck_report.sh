#!/bin/bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")" && pwd)
SOURCE_DIR=$(dirname "${ROOT_DIR}")
PROJECT_NAME=$(cat ${SOURCE_DIR}/CMakeLists.txt | grep "project(" | awk -F ' ' '{print $1}')
CPPCHECK_REPORT_DIR=${SOURCE_DIR}/output/cppcheck_report
CPPCHECK_SUPPRESS_LIST_FILE=${CPPCHECK_REPORT_DIR}/suppress_list_file.txt
CPPCHECK_XML_FILE=${CPPCHECK_REPORT_DIR}/cppcheck_result.xml

[ -d ${CPPCHECK_REPORT_DIR} ] && {
	rm -rf ${CPPCHECK_REPORT_DIR}
}
mkdir -p ${CPPCHECK_REPORT_DIR}
touch ${CPPCHECK_XML_FILE}

command -v cppcheck >/dev/null 2>&1 || {
	echo >&2 "cppcheck is not installed but required. Trying to install it..."
	sudo apt-get -y install cppcheck
}

echo "missingIncludeSystem" >${CPPCHECK_SUPPRESS_LIST_FILE}
# echo "missingInclude" >> ${CPPCHECK_SUPPRESS_LIST_FILE}
echo "preprocessorErrorDirective" >>${CPPCHECK_SUPPRESS_LIST_FILE}
# echo "invalidPrintArgType_sint" >> ${CPPCHECK_SUPPRESS_LIST_FILE}
# echo "nullPointer" >> ${CPPCHECK_SUPPRESS_LIST_FILE}
echo "objectIndex" >>${CPPCHECK_SUPPRESS_LIST_FILE}
echo "nullPointerRedundantCheck" >>${CPPCHECK_SUPPRESS_LIST_FILE}
echo "invalidscanf" >>${CPPCHECK_SUPPRESS_LIST_FILE}

cppcheck \
	--enable=all \
	--xml \
	--xml-version=2 \
	--inconclusive \
	--suppress=missingIncludeSystem \
	--project=${SOURCE_DIR}/build/compile_commands.json \
	--output-file=${CPPCHECK_XML_FILE} \
	--suppressions-list=${CPPCHECK_SUPPRESS_LIST_FILE}

[ -f ${CPPCHECK_XML_FILE} ] && {
	cppcheck-htmlreport \
		--file=${CPPCHECK_XML_FILE} \
		--title="${PROJECT_NAME}-$(date \+%F_%T)" \
		--report-dir=${CPPCHECK_REPORT_DIR} \
		--source-dir=${SOURCE_DIR}
}
