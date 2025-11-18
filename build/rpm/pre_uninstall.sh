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

# 检查服务是否存在
service_exists() {
    local service_name="$1"
    systemctl list-units --type=service --all | grep -q "$service_name"
}

# 停止并禁用服务
stop_and_disable_service() {
    local service_name="$1"

    if service_exists "$service_name"; then
        # 停止服务
        log_message "INFO" "Stopping $service_name service..."
        if ! systemctl stop "$service_name"; then
            handle_error "Failed to stop $service_name"
        fi

        # 禁用服务
        log_message "INFO" "Disabling $service_name service..."
        if ! systemctl disable "$service_name"; then
            handle_error "Failed to disable $service_name"
        fi
    else
        log_message "INFO" "$service_name does not exist, skipping stop and disable."
    fi
}

# 删除文件，如果文件不存在，则跳过
remove_file_if_exists() {
    local file="$1"

    if [ -f "$file" ]; then
        log_message "INFO" "Removing file $file..."
        if ! rm -f "$file"; then
            handle_error "Failed to remove $file"
        fi
    else
        log_message "INFO" "File $file does not exist, skipping removal."
    fi
}

# 卸载服务文件
uninstall_service() {
    # 检查服务文件是否存在并且已加载，然后重置失败状态
    if systemctl list-units --type=service | grep -q "ubturbo.service"; then
        # 如果服务已加载，重置失败状态
        systemctl reset-failed ubturbo.service || true
    fi
    local service_file="/etc/systemd/system/ubturbo.service"
    remove_file_if_exists "$service_file"
    # 重载 systemd 配置
    systemctl daemon-reload
}

# 主流程
main() {
# 检查是否是卸载操作
if [ "$1" -ne 0 ]; then
    log_message "INFO"  "Not an uninstall operation."
    chown "ubturbo:ubturbo" "/var/log/ubturbo/ubturbo-uninstall.log"
    exit 0
else
    log_message "INFO"  "Uninstalling the package pre..."
fi

log_message "$RPM_INSTALL"
log_message "INFO" "======================"
log_message "INFO" "pre_uninstall.sh started"
log_message "INFO" "======================"

# 停止并禁用服务
stop_and_disable_service "ubturbo.service"

# 卸载服务文件
uninstall_service

log_message "INFO" "======================"
log_message "INFO" "pre_uninstall.sh ended"
log_message "INFO" "======================"
}

# 执行主流程
main "$@"