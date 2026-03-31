// pipe_experiment.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>

// 管道相关常量
#define PIPE_READ_END 0
#define PIPE_WRITE_END 1
#define MAX_BUFFER_SIZE 65536  // 最大测试数据
#define CHUNK_SIZE 1024        // 每次读写块大小
#define NUM_CHILDREN 3         // 子进程数量

// 共享内存结构体
typedef struct {
    sem_t sem_write;     // 写信号量
    sem_t sem_read;      // 读信号量
    int write_complete;  // 写完成标志
    int bytes_written;  // 已写入字节数
} shared_data_t;

// 测试管道容量
int test_pipe_capacity() {
    int pipefd[2];
    int capacity = 0;
    char buffer[CHUNK_SIZE];

    // 填充缓冲区
    memset(buffer, 'A', CHUNK_SIZE);

    // 创建管道
    if (pipe(pipefd) == -1) {
        perror("管道创建失败");
        return -1;
    }

    printf("=== 测试管道容量 ===\n");

    // 设置为非阻塞写
    fcntl(pipefd[PIPE_WRITE_END], F_SETFL, O_NONBLOCK);

    // 不断写入直到管道满
    while (1) {
        int written = write(pipefd[PIPE_WRITE_END], buffer, CHUNK_SIZE);
        if (written > 0) {
            capacity += written;
        } else if (written == -1) {
            if (errno == EAGAIN) {
                printf("管道已满！\n");
                printf("管道容量: %d 字节 (%.2f KB)\n",
                       capacity, capacity / 1024.0);
                break;
            } else {
                perror("写入错误");
                break;
            }
        }
    }

    // 清理
    close(pipefd[PIPE_READ_END]);
    close(pipefd[PIPE_WRITE_END]);

    return capacity;
}

// 子进程：向管道写入数据
void child_process(int write_fd, int child_id, shared_data_t *shared) {
    srand(time(NULL) ^ (getpid() << 16));

    char message[256];
    sprintf(message, "子进程%d的消息: PID=%d, 时间戳=%ld\n",
            child_id, getpid(), time(NULL));

    int message_len = strlen(message);

    printf("子进程%d[PID:%d] 准备写入 %d 字节\n",
           child_id, getpid(), message_len);

    // 等待信号量（互斥访问管道）
    sem_wait(&shared->sem_write);

    printf("子进程%d 获得写权限\n", child_id);

    // 模拟一些处理时间
    usleep((rand() % 1000) * 1000);

    // 写入管道
    int written = write(write_fd, message, message_len);

    if (written > 0) {
        printf("子进程%d 写入 %d 字节: %s",
               child_id, written, message);
        shared->bytes_written += written;
    } else if (written == -1) {
        if (errno == EAGAIN) {
            printf("子进程%d: 管道已满，写入被阻塞\n", child_id);

            // 测试阻塞情况
            written = write(write_fd, message, message_len);
            if (written > 0) {
                printf("子进程%d 最终写入 %d 字节\n", child_id, written);
                shared->bytes_written += written;
            }
        } else {
            perror("写入失败");
        }
    }

    // 释放信号量
    sem_post(&shared->sem_write);

    // 通知父进程本进程写入完成
    __sync_fetch_and_add(&shared->write_complete, 1);

    // 短暂延迟后退出
    usleep(500000);

    printf("子进程%d 完成，退出\n", child_id);
    exit(0);
}

// 父进程：从管道读取数据
void parent_process(int read_fd, shared_data_t *shared) {
    char buffer[CHUNK_SIZE];
    int total_read = 0;
    int read_count = 0;
    
    printf("\n父进程[PID:%d] 等待所有子进程完成写入...\n", getpid());
    
    // 等待所有子进程完成写入
    while (shared->write_complete < NUM_CHILDREN) {
        usleep(100000);  // 100ms检查一次
    }
    
    printf("所有子进程已完成写入，开始读取...\n");
    
    // 设置为非阻塞读，测试非阻塞情况
    fcntl(read_fd, F_SETFL, O_NONBLOCK);
    
    // 尝试读取，直到没有数据
    while (1) {
        int n = read(read_fd, buffer, sizeof(buffer) - 1);
        
        if (n > 0) {
            buffer[n] = '\0';
            printf("父进程读取 %d 字节: \n%s", n, buffer);
            total_read += n;
            read_count++;
        } else if (n == 0) {
            printf("管道已读取完毕\n");
            break;
        } else if (n == -1) {
            if (errno == EAGAIN) {
                printf("管道为空，等待数据...\n");
                
                // 切换到阻塞模式
                fcntl(read_fd, F_SETFL, 0);
                printf("切换到阻塞模式，继续读取...\n");
                continue;
            } else {
                perror("读取错误");
                break;
            }
        }
        
        // 模拟处理时间
        usleep(200000);
    }
    
    printf("\n=== 读取统计 ===\n");
    printf("总读取次数: %d\n", read_count);
    printf("总读取字节: %d\n", total_read);
    printf("总写入字节: %d\n", shared->bytes_written);
    printf("丢失字节数: %d\n", shared->bytes_written - total_read);
}

int main() {
    int pipefd[2];
    pid_t pids[NUM_CHILDREN];
    int pipe_capacity;

    printf("========================================\n");
    printf("      管道通信实验程序\n");
    printf("      测试管道通信、阻塞、容量\n");
    printf("========================================\n\n");

    // 1. 测试管道默认容量
    pipe_capacity = test_pipe_capacity();
    if (pipe_capacity <= 0) {
        printf("管道容量测试失败\n");
        return 1;
    }

    printf("\n=== 创建主通信管道 ===\n");

    // 2. 创建管道
    if (pipe(pipefd) == -1) {
        perror("管道创建失败");
        return 1;
    }

    // 3. 创建共享内存和信号量
    shared_data_t *shared = mmap(NULL, sizeof(shared_data_t),
                                 PROT_READ | PROT_WRITE,
                                 MAP_SHARED | MAP_ANONYMOUS,
                                 -1, 0);
    if (shared == MAP_FAILED) {
        perror("共享内存创建失败");
        return 1;
    }

    // 初始化共享数据
    memset(shared, 0, sizeof(shared_data_t));

    // 初始化信号量
    if (sem_init(&shared->sem_write, 1, 1) == -1) {  // 二进制信号量
        perror("信号量初始化失败");
        return 1;
    }

    sem_init(&shared->sem_read, 1, 0);
    shared->write_complete = 0;
    shared->bytes_written = 0;

    // 4. 创建3个子进程
    printf("\n=== 创建子进程 ===\n");
    for (int i = 0; i < NUM_CHILDREN; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork失败");
            return 1;
        } else if (pid == 0) {
            // 子进程
            close(pipefd[PIPE_READ_END]);  // 关闭不用的读端
            child_process(pipefd[PIPE_WRITE_END], i + 1, shared);
        } else {
            // 父进程记录子进程PID
            pids[i] = pid;
            printf("创建子进程%d, PID=%d\n", i + 1, pid);
        }
    }

    // 5. 父进程代码
    if (getpid() != 0) {  // 确保是父进程
        // 关闭不用的写端
        close(pipefd[PIPE_WRITE_END]);

        // 等待所有子进程写入完成
        parent_process(pipefd[PIPE_READ_END], shared);

        // 等待子进程退出
        printf("\n=== 等待子进程退出 ===\n");
        for (int i = 0; i < NUM_CHILDREN; i++) {
            int status;
            waitpid(pids[i], &status, 0);
            printf("子进程%d 已退出，状态: %d\n",
                   i + 1, WEXITSTATUS(status));
        }

        // 清理信号量
        sem_destroy(&shared->sem_write);
        sem_destroy(&shared->sem_read);

        // 清理共享内存
        munmap(shared, sizeof(shared_data_t));

        printf("\n=== 实验完成 ===\n");
    }

    return 0;
}
