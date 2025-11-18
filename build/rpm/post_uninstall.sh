#!/bin/bash

# 默认日志目录和日志文件
LOG_DIR="/var/log/ubturbo"
LOG_FILE="$LOG_DIR/ubturbo-uninstall.log"

# 检查并创建日志目录
create_log_directory() {
  # 只检查一次，减少不必要的文件系统操作
  if [ ! -d "$LOG_DIR" ]; then
    mkdir -p "$LOG_DIR" || handle_error "Failed to create log directory $LOG_DIR"
    chmod 700 "$LOG_DIR"  # 设置日志目录权限为700
    log_message "INFO" "Log directory $LOG_DIR created successfully."
  elif [ ! -w "$LOG_DIR" ]; then
    handle_error "Log directory $LOG_DIR is not writable."
  fi
}

# 输出日志到文件和控制台
log_message() {
  local log_type="$1"
  local message="$2"

  # 获取时间戳
  local timestamp
  timestamp="$(date)"
  local log_entry="[$timestamp] [$log_type] $message"

  # 检查日志目录是否存在并且可写
  create_log_directory

  # 如果日志文件是首次创建，则设置权限为600
  if [ ! -f "$LOG_FILE" ]; then
    touch "$LOG_FILE"
    chmod 600 "$LOG_FILE"  # 只在首次创建时设置权限
  fi

  # 写入日志文件
  echo "$log_entry" >> "$LOG_FILE"
}

# 统一错误处理
handle_error() {
    log_message "ERROR" "$1"
    exit 1
}

# 调用示例：
# log_message "INFO" "This is an info message."
# log_message "ERROR" "This is an error message."
# log_message "DEBUG" "This is a debug message."

# 删除指定目录（如果存在）
remove_directory_if_exists() {
    local dir="$1"
    if [ -d "$dir" ]; then
        log_message "INFO" "Removing directory $dir..."
        rm -rf "$dir" || handle_error "Failed to remove directory $dir"
    else
        log_message "INFO" "Directory $dir does not exist, skipping removal."
    fi
}

# 删除日志目录
remove_log_directory() {
    local dir="/var/log/ubturbo"
    if [ -d "$dir" ]; then
        rm -rf "$dir" || echo "Failed to remove directory $dir"
    else
        echo "Directory $dir does not exist, skipping removal."
    fi
}

# 删除用户及用户组
remove_user_and_group() {
    local user="ubturbo"
    local group="ubturbo"

    # 删除用户
    if id "$user" &>/dev/null; then
        log_message "INFO" "Removing user $user..."
        userdel -r "$user" >/dev/null 2>> "$LOG_FILE"
    else
        log_message "INFO" "User $user does not exist, skipping removal."
    fi

    # 删除用户组
    if getent group "$group" &>/dev/null; then
        log_message "INFO" "Removing group $group..."
        groupdel "$group" >/dev/null 2>> "$LOG_FILE" || handle_error "Failed to remove group $group"
    else
        log_message "INFO" "Group $group does not exist, skipping removal."
    fi
}

# 去除字符串前后空白字符的函数
trim_spaces() {
    local input="$1"
    # 使用 awk 去除首尾空格
    input=$(echo "$input" | awk '{gsub(/^[ \t]+/, ""); gsub(/[ \t]+$/, ""); print}')
    echo "$input"
}

# 主流程
main() {
# 检查是否是卸载操作
if [ "$1" -ne 0 ]; then
    log_message "INFO"  "Not an uninstall operation."
    chown "ubturbo:ubturbo" "/var/log/ubturbo/ubturbo-uninstall.log"
    exit 0
else
    log_message "INFO"  "Uninstalling the package post..."
fi
log_message "INFO" "======================"
log_message "INFO" "post_uninstall.sh started"
log_message "INFO" "======================"

# 删除用户组
remove_user_and_group

log_message "INFO" "======================"
log_message "INFO" "post_uninstall.sh ended"
log_message "INFO" "======================"
# 删除日志目录
remove_log_directory
}

# 执行主流程
main "$@"
