# Makefile
CC = gcc
CFLAGS = -Wall -g
TARGETS = myshell cmd1 cmd2 cmd3

all: $(TARGETS)

myshell: myshell.c
	$(CC) $(CFLAGS) -o myshell myshell.c

cmd1: cmd1.c
	$(CC) $(CFLAGS) -o cmd1 cmd1.c

cmd2: cmd2.c
	$(CC) $(CFLAGS) -o cmd2 cmd2.c

cmd3: cmd3.c
	$(CC) $(CFLAGS) -o cmd3 cmd3.c

clean:
	rm -f $(TARGETS) *.o

test: all
	@echo "=== 编译完成 ==="
	@echo "可执行文件: myshell, cmd1, cmd2, cmd3"
	@echo "=== 运行示例 ==="
	@echo "1. ./myshell"
	@echo "2. 在myshell中执行: cmd1"
	@echo "3. 在myshell中执行: cmd2"
	@echo "4. 在myshell中执行: cmd3 myshell.c"
	@echo "5. 在myshell中执行: exit 退出"
