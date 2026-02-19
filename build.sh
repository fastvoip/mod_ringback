#!/bin/bash
# mod_ringback 构建脚本
# 支持两种构建方式：
# 1. 在 FreeSWITCH 源码树中: ./build.sh /path/to/freeswitch
# 2. 使用已安装的 FreeSWITCH: ./build.sh (使用 /usr/local/freeswitch)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

if [ -n "$1" ]; then
    FS_SRC="$1"
    echo "使用 FreeSWITCH 源码: $FS_SRC"
    make FS_SRC="$FS_SRC" FS_PREFIX="${FS_PREFIX:-/usr/local/freeswitch}"
else
    # 尝试常见路径
    for PREFIX in /usr/local/freeswitch /usr/include/freeswitch; do
        if [ -d "$PREFIX" ] || [ -d "$(dirname $PREFIX)" ]; then
            if [ -f "/usr/local/freeswitch/include/switch.h" ] || [ -f "/usr/include/freeswitch/switch.h" ]; then
                echo "使用已安装的 FreeSWITCH"
                make
                exit 0
            fi
        fi
    done
    echo "错误: 未找到 FreeSWITCH。请指定 FreeSWITCH 源码路径: ./build.sh /path/to/freeswitch"
    exit 1
fi
