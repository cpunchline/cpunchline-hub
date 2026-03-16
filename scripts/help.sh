#!/bin/bash
set -euo pipefail

# sub shell run
# (function)

# background run
bkr() {
	(nohup "$@" &>/dev/null &)
}

traverse_files_in_dir() {
	local dir="$1"
	local suffix="$2"
	files_list=""
	for absolute_file in $(ls ${dir}/*${suffix}); do
		relative_file="$(basename ${absolute_file} '${suffix}')"
		files_list="${files_list}|${relative_file}"
	done
	logwarn "${files_list:1}"
}

check_and_install_package() {
	local package=$1
	local install_package=$2
	command -v ${package} >/dev/null 2>&1 || {
		echo >&2 "${install_package} is not installed but required. Trying to install it..."
		sudo apt-get -y install ${install_package}
	}
}

get_functions() {
	declare -F | awk '{print $3}'
}
