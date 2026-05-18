#!/bin/bash
# test.sh - 测试脚本

echo "=== 开始测试模拟Shell ==="
echo ""
echo "测试1: 编译所有程序"
make clean
make
echo ""

echo "测试2: 直接运行cmd1"
./cmd1
echo ""

echo "测试3: 直接运行cmd2 (输入 5 + 3 然后输入 q)"
echo "5 + 3" | ./cmd2
echo ""

echo "测试4: 直接运行cmd3"
./cmd3 cmd1.c
echo ""

echo "测试5: 运行模拟Shell"
echo "在接下来的交互中，请手动测试以下命令:"
echo "1. cmd1"
echo "2. cmd2"
echo "3. cmd3 myshell.c"
echo "4. ls -la"
echo "5. pwd"
echo "6. invalid_cmd (应该显示Command not found)"
echo "7. exit"
echo ""

./myshell
