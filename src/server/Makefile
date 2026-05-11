# 编译器
CC = gcc

# 编译选项：-pthread 必须加在 CFLAGS 中以确保编译时正确启用线程特性
CFLAGS = -Wall -Wextra -g -I../include -pthread

# 链接选项
LDFLAGS = -pthread

# 目标可执行文件
TARGET = server

# 源文件与目标文件（根据代码仓 server 目录下的实际文件补全）
SRCS = server.c thr_channel.c thr_list.c medialib.c mytbf.c
OBJS = $(SRCS:.c=.o)

# 默认目标：构建可执行文件
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# 模式规则：将 .c 文件编译为 .o 文件
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 清理规则
clean:
	rm -f $(OBJS) $(TARGET)

# 伪目标声明
.PHONY: all clean
