#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

// 系统调用号
#define GET_FILEINFO_NUM 454

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file_path>\n", argv[0]);
        return 1;
    }
    
    const char *filepath = argv[1];
    
    // 调用系统调用
    long ret = syscall(GET_FILEINFO_NUM, filepath);
    
    if (ret < 0) {
        perror("System call failed");
        printf("Error code: %ld\n", ret);
        return 1;
    }
    
    printf("System call succeeded. Check kernel logs with:\n");
    printf("  sudo dmesg | tail -20\n");
    
    return 0;
}

