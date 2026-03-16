#!/bin/bash

# White
loginfo() {
	printf "\033[37m$1\033[0m\n" "$1"
}

# Yellow
logwarn() {
	printf "\033[33m$1\033[0m\n" "$1"
}

# Green
logok() {
	printf "\033[32m$1\033[0m\n" "$1"
}

# Red
logerr() {
	printf "\033[31m$1\033[0m\n" "$1"
}

# Blue
logblue() {
	printf "\033[34m$1\033[0m\n" "$1"
}

NANOPB_LIMIT_MSG_MAX_SIZE=$((8 * 1024))
ROOT_DIR=$(cd "$(dirname "$0")" && pwd)
NANOPB_GENERATOR_DIR="$1"
PROTOC_GEN_NANOPB_DIR="$2"
NANOPB_PROTO_DIR="$3"
NANOPB_GENERATED_CODE_DIR="$4"

command -v protoc >/dev/null 2>&1 || {
	loginfo >&2 "protobuf-compiler is not installed but required. Trying to install it..."
	sudo apt-get -y install protobuf-compiler python3-protobuf
}

: '
REGENERATOR_FLAG=1 # 0: no changes, 1: changes
NANOPB_CONFIG_CHANGES=$(git ls-files --modified --others ${NANOPB_PROTO_DIR} | wc -l)
if [ ${NANOPB_CONFIG_CHANGES} -eq 0 ]; then
	if [ -d ${NANOPB_GENERATED_CODE_DIR} ]; then
		NANOPB_GENERATED_CODE_CHANGES=$(git ls-files --modified --others ${NANOPB_GENERATED_CODE_DIR} | wc -l)
		if [ ${NANOPB_GENERATED_CODE_CHANGES} -eq 0 ]; then
			REGENERATOR_FLAG=0
		fi
	fi
fi
'

#if [ ${REGENERATOR_FLAG} -ne 0 ]; then
#logwarn "-- found changes to re-generated."
cd ${NANOPB_PROTO_DIR}

for i in $(ls ${NANOPB_PROTO_DIR}/*.proto); do
	logblue "-- generating code from $i"
	module_name="$(basename $i .proto)"
	rm -f ${NANOPB_GENERATED_CODE_DIR}/${module_name}.pb.h
	rm -f ${NANOPB_GENERATED_CODE_DIR}/${module_name}.pb.c
	protoc \
		--plugin=protoc-gen-nanopb="${PROTOC_GEN_NANOPB_DIR}/protoc-gen-nanopb" \
		--proto_path="${NANOPB_PROTO_DIR}" \
		--proto_path="${NANOPB_GENERATOR_DIR}/proto" \
		--nanopb_out="${NANOPB_GENERATED_CODE_DIR}" \
		$i

	# limit the max msg size
	H_FILE="${NANOPB_GENERATED_CODE_DIR}/${module_name}.pb.h"
	macros=$(grep -h "#define [A-Z0-9_]*_PB_H_MAX_SIZE" "${H_FILE}" 2>/dev/null | awk '{print $2}')
	if [ -n "${macros}" ]; then
		for macro in ${macros}; do
			val=$(grep "#define ${macro} " "${H_FILE}" | head -1 | awk '{print $3}')

			while true; do
				case "${val}" in
				[A-Za-z_]*)
					next_val=$(grep "#define ${val} " "${H_FILE}" | head -1 | awk '{print $3}')
					if [ -z "${next_val}" ] || [ "${next_val}" = "${val}" ]; then
						break
					fi
					val="${next_val}"
					;;
				*)
					break
					;;
				esac
			done

			clean_value=$(echo "${val}" | tr -cd '0-9')

			if [ -z "${clean_value}" ]; then
				logwarn "Cannot resolve numeric value for ${macro} (got: '${val}'). Skipping check."
				continue
			fi

			if [ "${clean_value}" -gt "${NANOPB_LIMIT_MSG_MAX_SIZE}" ]; then
				logerr "FATAL: Module '${module_name}' exceeds size limit!"
				logerr "  Macro: ${macro} = ${clean_value} bytes"
				logerr "  Limit: ${NANOPB_LIMIT_MSG_MAX_SIZE} bytes (16KB)"
				logerr "Action: Please reduce message size or increase limit if absolutely necessary."
				exit 1
			fi
		done
		logok "  [OK] All messages in ${module_name} are within limit (${NANOPB_LIMIT_MSG_MAX_SIZE} bytes)."
	else
		loginfo "  [INFO] No *_PB_H_MAX_SIZE macros found in ${module_name} (skipping check)."
	fi
done
#else
#	logwarn "-- no changes to re-generated."
#fi

cd -
