#!/bin/bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")" && pwd)
SOURCE_DIR=$(dirname "${CURRENT_DIR}")

if [ $# -eq 0 ]; then
	echo "usage: $0 file/directory/gitfile"
	exit 1
fi

command -v clang-format >/dev/null 2>&1 || {
	echo >&2 "clang-format is not installed but required. Trying to install it..."
	sudo apt-get -y install clang-format
}

command -v shfmt >/dev/null 2>&1 || {
	echo >&2 "shfmt is not installed but required. Trying to install it..."
	sudo apt-get -y install shfmt
}

if [ "$1" == "gitfile" ]; then
	modified_c_files=$(git status --porcelain | cut -c4- | grep -E '\.(h|hpp|hxx|c|cpp|cxx)$')
	if [ -n "$modified_c_files" ]; then
		clang-format -i -style=file "${modified_c_files}"
	else
		echo "git no c/c++ file!"
		exit 1
	fi

	modified_sh_files=$(git status --porcelain | cut -c4- | grep -E '\.(sh)$')
	if [ -n "$modified_sh_files" ]; then
		shfmt -w "${modified_sh_files}"
	else
		echo "git no sh file!"
		exit 1
	fi
elif [ -d "$1" ]; then
	echo "format the directory $1"
	find "$1" -regex '.*\.\(h\|hpp\|hxx\|c\|cpp\|cxx\)' -exec clang-format -i -style=file {} \;
	find "$1" -regex '.*\.\(sh\)' -exec shfmt -w {} \;
elif [ -f "$1" ]; then
	echo "format the file $1"
	case "$1" in
	*.sh)
		shfmt -w "$1"
		;;
	*.h | *.hpp | *.hxx | *.c | *.cc | *.cpp | *.cxx)
		clang-format -i -style=file "$1"
		;;
	*)
		echo "not support to format $1"
		;;
	esac
else
	echo "invalid param $1"
	exit 1
fi

exit 0
