#!/bin/bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")" && pwd)

show_running_time() {
	local uptimes=$(cat /proc/uptime | awk '{print int($1)}')
	local days=$((uptimes / 86400))
	local hours=$((uptimes % 86400 / 3600))
	local minutes=$((uptimes % 3600 / 60))
	local seconds=$((uptimes % 60))

	echo "system running ${days} day ${hours} hour ${minutes} min ${seconds} s"
}

open_coredump() {
	local coredump_dir=$1
	echo "--> set coredump to ${coredump_dir}"
	[ ! -d "${coredump_dir}" ] && {
		mkdir -p "${coredump_dir}"
	}
	echo "${coredump_dir}/%e_%p_%t_%s.core" >/proc/sys/kernel/core_pattern
	ulimit -c unlimited
}

check_coredump() {
	local coredump_dir=$1
	local coredump_count=$(ls "${coredump_dir}"/*.core 2>/dev/null | wc -l)
	[ ${coredump_count} -gt 10 ] && {
		local rm_count=$((coredump_count - 10))
		echo "coredump_count=${coredump_count}, need to delete ${rm_count} files"
		ls -tr "${coredump_dir}"/*.core 2>/dev/null | head -n ${rm_count} | xargs rm -f
	}
}

open_tcpdump() {
	command -v tcpdump >/dev/null 2>&1 || {
		echo >&2 "tcpdump is not installed but required. Trying to install it..."
		sudo apt-get -y install tcpdump
	}

	local tcpdump_dir=$1
	local tcpdump_ethernet_interface=$2
	local tcpdump_file_size_MB_max=$3
	local tcpdump_file_count_max=$4
	echo "--> set tcpdump to ${tcpdump_dir}"
	[ ! -d "${tcpdump_dir}" ] && {
		mkdir -p "${tcpdump_dir}"
	}
	local cur_time=$(date +%Y%m%d%H%M%S)
	nohup tcpdump \
		-i ${tcpdump_ethernet_interface} \
		-C ${tcpdump_file_size_MB_max} \
		-W ${tcpdump_file_count_max} \
		-s 150 \
		-U \
		-w ${tcpdump_dir}/tcpdump_${cur_time}.cap >/dev/null \
		2>&1 &
}

check_tcpdump() {
	local tcpdump_dir=$1
	local tcpdump_file_count_max=$2
	local tcpdump_cur_file_count=$(ls -tr "${tcpdump_dir}"/tcpdump*.cap* 2>/dev/null | wc -l)
	if [ ${tcpdump_cur_file_count} -gt "${tcpdump_file_count_max}" ]; then
		local to_delete=$((tcpdump_cur_file_count - tcpdump_file_count_max))
		ls -tr "${tcpdump_dir}"/tcpdump*.cap* 2>/dev/null | head -n ${to_delete} | xargs rm -f
	fi
}

check_dir_space() {
	local dir=$1
	local size=$(du -sh "${dir}" 2>/dev/null | awk '{print $1}')
	echo "[${dir}] space is [${size}]"

	# df -Th
	# df -Th | awk 'NR==1 || !seen[$1]++'
}

check_ping_ip() {
	local ip=$1
	ping ${ip} -c 3 -W 1 -s 1 | grep -E "ttl|packet|PING" # check ethernet
	ping ${ip} -c 1 -W 3 | grep -E "ttl|packet|PING"      # check network
}

check_ethernet() {
	command -v ifconfig >/dev/null 2>&1 || {
		echo >&2 "ifconfig is not installed but required. Trying to install it..."
		sudo apt-get -y install net-tools
	}

	local ethernet_interface=$1
	ifconfig
	ifconfig | grep ${ethernet_interface} >/dev/null || {
		echo "--> !!! ${ethernet_interface} down !!!"
		TZ=CST-8 date >"/tmp/err_${ethernet_interface}_down"
	}
}

check_memory() {
	local memval=$(grep MemAvailable /proc/meminfo | awk '{print $2}')
	[ ! -e "/tmp/memval" ] && {
		echo "${memval}" >/tmp/memval
	}
	local memval_last=$(cat /tmp/memval)
	local meminterval=$((memval_last - memval))
	if [ "${meminterval}" -ge 20480 ] || [ "${memval}" -lt 102400 ]; then
		echo "${memval}" >/tmp/memval
		echo "--> !!! Memory leak, left ${memval} KB, interval ${meminterval} KB !!!"
		TZ=CST-8 date >"/tmp/err_memleak"
	fi
}

check_interrupts() {
	cat /proc/interrupts | awk '$2!=0'
}

check_process_fd() {
	local process_name=$1
	local pid=$(pidof ${process_name} 2>/dev/null) || return 0
	local pid_list=$(ps -o pid,ppid,comm | grep ${pid} | awk '{print $1}')
	for p in ${pid_list}; do
		local fd_count=$(lsof -p ${p} 2>/dev/null | wc -l)
		if [ ${fd_count} -gt 150 ]; then
			local greater_process_name=$(ps -o pid,comm | grep ${p} | awk '{print $2}')
			echo "process_name=${greater_process_name}, pid=${p}, fd_count=${fd_count}"
			if [ ${fd_count} -gt 512 ]; then
				lsof -p ${p}
			fi
			if [ ${fd_count} -gt 1000 ]; then
				echo "kill ${greater_process_name}, because it use fd count greater than 1000"
				kill -9 ${p}
			fi
		fi
	done
}

check_process() {
	top -b -n 1 | awk '$8!=0'
	ps -o pid,ppid,vsz,rss,comm | awk '$4>3000'
}

check_dir_space $1