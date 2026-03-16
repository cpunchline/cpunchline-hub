#!/bin/sh

cur_path=$(
	cd "$(dirname "$0")"
	pwd
)
top_path=$(git rev-parse --show-toplevel 2>/dev/null) || {
	echo "Not in git repo"
	exit 1
}

template_file="${cur_path}/.gitmessage"
hook_file="${top_path}/.git/hooks/commit-msg"
script_hook="${cur_path}/commit-msg"

case "$1" in
set)
	[ ! -f "${template_file}" ] && {
		echo "Template missing: ${template_file}"
		exit 1
	}
	git config commit.template "${template_file}"
	rm -f "${hook_file}" && ln -s "${script_hook}" "${hook_file}"
	echo "Commit template set"
	;;
unset)
	current=$(git config commit.template)
	[ "${current}" = "${template_file}" ] && git config --unset commit.template
	[ -L "${hook_file}" ] && rm -f "${hook_file}"
	echo "Commit template unset"
	;;
*)
	echo "Usage: $0 {set|unset}"
	exit 1
	;;
esac
