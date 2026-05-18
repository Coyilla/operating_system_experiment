#!/bin/bash

# 调试测试脚本
echo "=== 调试测试脚本 ==="

# 编译
gcc -o filesystem filesystem.c

# 创建测试输入
cat > debug_test.txt << 'EOF'
help
mkdir testdir
cd testdir
ls
create test1.txt
open test1.txt
write 0
0
This is test file 1.
.
close 0
open test1.txt
read 0 50
close 0
ls
rm test1.txt
ls
cd ..
rmdir testdir
ls
exit
y
EOF

echo "运行调试测试..."
./filesystem < debug_test.txt

echo "=== 大文件测试 ==="
cat > largefile_test.txt << 'EOF'
help
mkdir large_test
cd large_test
create big.txt
open big.txt
write 0
0
EOF

# 生成超过1024字节的内容
for i in {1..50}; do
    echo "Line $i: This is a line of text to fill up the file. We need enough content to span multiple blocks. Each line is about 80 characters, so 50 lines is about 4000 bytes, which should require 4 blocks." >> largefile_test.txt
done

cat >> largefile_test.txt << 'EOF'
.
close 0
ls
open big.txt
read 0 500
close 0
rm big.txt
ls
cd ..
rmdir large_test
ls
exit
y
EOF

echo "运行大文件测试..."
./filesystem < largefile_test.txt
