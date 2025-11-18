#!/bin/bash
# 用法: cat.sh <pid> [<filename>]
# 输出: /proc/<pid>/<filename> 的内容，如果没有提供 <filename>，默认为 numa_maps

echo "UID=$(id -u), EUID=$(id -ru)" >&2

if [ $# -lt 1 ]; then
  echo "Usage: $0 <pid> [<filename>]" >&2
  exit 1
fi

PID=$1
FNAME=${2:-numa_maps}  # 如果第二个参数为空，使用默认值 numa_maps

# 校验 PID 必须是数字
if ! [[ "$PID" =~ ^[0-9]+$ ]]; then
  echo "Invalid input: PID must be a numeric value" >&2
  exit 1
fi

# 可白名单限制
ALLOWED_FILES=("numa_maps" "smaps" "status" "maps" "cmdline" "environ" "comm")
valid=0
for file in "${ALLOWED_FILES[@]}"; do
    if [[ "$FNAME" == "$file" ]]; then
        valid=1
        break
    fi
done
if [[ $valid -ne 1 ]]; then
    echo "Invalid filename: $FNAME is not allowed" >&2
    exit 1
fi

FILE="/proc/$PID/$FNAME"

if [ -f "$FILE" ]; then
  /usr/bin/cat "$FILE"
else
  echo "File not found: $FILE" >&2
  exit 1
fi