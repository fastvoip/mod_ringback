# mod_ringback - FreeSWITCH 回铃音识别模块 Makefile
# 需要在 FreeSWITCH 源码树中编译，或设置正确的 FreeSWITCH 路径

# FreeSWITCH 安装路径 (如果已安装)
FS_PREFIX ?= /usr/local/freeswitch

# 或使用 FreeSWITCH 源码路径 (从源码编译时)
# FS_SRC ?= /path/to/freeswitch

# 检测 FreeSWITCH 路径
ifdef FS_SRC
    FS_INC := $(FS_SRC)/src/include
    FS_MOD := $(FS_PREFIX)/mod
else
    FS_INC := $(FS_PREFIX)/include
    FS_MOD := $(FS_PREFIX)/mod
endif

# 编译器
CC = gcc
CFLAGS = -fPIC -shared -Wall -I$(FS_INC)
LDFLAGS = -shared

# 源文件
SRC = src/mod_ringback.c
TARGET = mod_ringback.so

.PHONY: all clean install test

all: $(TARGET)

test:
	$(MAKE) -C test test

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) -lm

install: $(TARGET)
	install -m 644 $(TARGET) $(FS_MOD)/

clean:
	rm -f $(TARGET)
