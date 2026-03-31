#!/bin/bash
# test_script.sh

# 清理IPC
make cleanipc

# 编译
make

echo "=== 启动receiver ==="
./receiver &
RECEIVER_PID=$!
echo "Receiver PID: $RECEIVER_PID"

sleep 2  # 等待receiver启动

echo ""
echo "=== 启动sender ==="
echo "输入测试消息: Hello World from Sender!"
echo "Hello World from Sender!" | timeout 5 ./sender

# 等待进程结束
wait $RECEIVER_PID
