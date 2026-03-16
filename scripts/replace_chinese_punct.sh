#!/bin/bash
# replace_chinese_punct.sh - 替换代码文件中的中文标点符号为英文标点
# Usage: ./replace_chinese_punct.sh <file_or_directory> [file_extensions]
# Example: ./replace_chinese_punct.sh ./src
# Example: ./replace_chinese_punct.sh ./src "c,cpp,h,hpp"

set -e

# 默认处理的代码文件扩展名
DEFAULT_EXTENSIONS="c,cpp,h,hpp,cc,cxx,java,py,js,ts,go,rs,sh,bash,zsh,md,cmake,txt"

# 中文标点 -> 英文标点 替换规则
# 注意: sed 命令顺序很重要，先处理长的再处理短的
PUNCT_RULES='s/——/--/g; s/……/.../g; s/，/,/g; s/。/./g; s/；/;/g; s/：/:/g; s/？/?/g; s/！/!/g; s/（/(/g; s/）/)/g; s/【/[/g; s/】/]/g; s/《/</g; s/》/>/g; s/、/,/g; s/·/`/g; s/～/~/g; s/—/-/g'

# 显示帮助
show_help() {
	echo "Usage: $0 <file_or_directory> [file_extensions]"
	echo ""
	echo "Arguments:"
	echo "  file_or_directory    要处理的文件或目录路径"
	echo "  file_extensions      可选，逗号分隔的文件扩展名列表 (默认: $DEFAULT_EXTENSIONS)"
	echo ""
	echo "Examples:"
	echo "  $0 ./src                          # 处理 src 目录下所有代码文件"
	echo "  $0 ./src \"c,cpp,h\"               # 只处理 .c .cpp .h 文件"
	echo "  $0 ./main.c                       # 处理单个文件"
	echo "  $0 ./docs \"md,txt\"               # 处理文档文件"
	echo ""
	echo "Note: 第三方库目录 (thirdparty, .git, build, output, .cache) 会被自动排除"
}

# 检查参数
if [ $# -lt 1 ] || [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
	show_help
	exit 0
fi

TARGET="$1"
EXTENSIONS="${2:-$DEFAULT_EXTENSIONS}"

# 构建 find 的 -name 参数
build_name_args() {
	local exts="$1"
	local first=1
	local result=""

	IFS=',' read -ra EXT_ARR <<<"$exts"
	for ext in "${EXT_ARR[@]}"; do
		ext=$(echo "$ext" | xargs) # 去除空格
		if [ $first -eq 1 ]; then
			result="-name \"*.$ext\""
			first=0
		else
			result="$result -o -name \"*.$ext\""
		fi
	done

	echo "$result"
}

# 处理单个文件
process_file() {
	local file="$1"

	# 检查文件是否存在且可读
	if [ ! -f "$file" ] || [ ! -r "$file" ]; then
		echo "Skip: Cannot read $file"
		return
	fi

	# 检查文件是否为文本文件
	if ! file "$file" | grep -qE "text|ASCII|UTF-8"; then
		echo "Skip: Not a text file $file"
		return
	fi

	# 创建临时文件
	local tmpfile=$(mktemp)

	# 执行替换
	if sed -e "$PUNCT_RULES" "$file" >"$tmpfile" 2>/dev/null; then
		# 检查是否有变化
		if ! diff -q "$file" "$tmpfile" >/dev/null 2>&1; then
			mv "$tmpfile" "$file"
			echo "Modified: $file"
		else
			rm "$tmpfile"
			# echo "No change: $file"
		fi
	else
		rm "$tmpfile"
		echo "Error processing: $file"
	fi
}

# 处理目录
process_directory() {
	local dir="$1"
	local exts="$2"

	echo "Processing directory: $dir"
	echo "File extensions: $exts"
	echo ""

	# 构建扩展名过滤条件
	local name_args=$(build_name_args "$exts")

	# 使用 find 查找并处理文件
	# 排除常见第三方目录
	local count=0
	local modified=0

	while IFS= read -r file; do
		if [ -n "$file" ]; then
			process_file "$file"
			count=$((count + 1))
			if [ "$?" -eq 0 ] && [ -f "$file.tmp" ]; then
				modified=$((modified + 1))
			fi
		fi
	done < <(eval "find '$dir' -type f \( $name_args \) \
        -not -path '*/thirdparty/*' \
        -not -path '*/.git/*' \
        -not -path '*/build/*' \
        -not -path '*/output/*' \
        -not -path '*/.cache/*' \
        -not -path '*/node_modules/*' \
        -not -path '*/.venv/*' \
        -not -path '*/venv/*' \
		-not -path '*/pre_built/*' \
        -not -path '*/replace_chinese_punct.sh' 2>/dev/null")

	echo ""
	echo "Summary: Processed $count files"
}

# 主逻辑
if [ -f "$TARGET" ]; then
	# 处理单个文件
	echo "Processing single file: $TARGET"
	process_file "$TARGET"
elif [ -d "$TARGET" ]; then
	# 处理目录
	process_directory "$TARGET" "$EXTENSIONS"
else
	echo "Error: '$TARGET' is not a valid file or directory"
	exit 1
fi

echo "Done!"
