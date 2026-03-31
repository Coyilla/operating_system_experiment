# Makefile
CC = gcc
CFLAGS = -Wall -g
LDFLAGS = 
TARGETS = sender receiver
OBJECTS = sem_utils.o

all: $(TARGETS)

# 编译工具函数
sem_utils.o: sem_utils.c common.h
	$(CC) $(CFLAGS) -c sem_utils.c

# 编译sender
sender: sender.c common.h sem_utils.o
	$(CC) $(CFLAGS) -o sender sender.c sem_utils.o $(LDFLAGS)

# 编译receiver
receiver: receiver.c common.h sem_utils.o
	$(CC) $(CFLAGS) -o receiver receiver.c sem_utils.o $(LDFLAGS)

# 清理
clean:
	rm -f $(TARGETS) $(OBJECTS) core

# 清理所有IPC资源
cleanipc:
	-ipcrm -a
	echo "IPC资源已清理"

# 测试运行
test: cleanipc all
	@echo "测试共享内存通信："
	@echo "1. 在一个终端运行: ./receiver"
	@echo "2. 在另一个终端运行: ./sender"
	@echo ""

# 显示IPC状态
status:
	@echo "当前IPC状态"
	@echo "共享内存:"
	@ipcs -m
	@echo ""
	@echo "信号量:"
	@ipcs -s
	@echo ""
	@echo "消息队列:"
	@ipcs -q

.PHONY: all clean cleanipc test status
