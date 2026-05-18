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

#define PIPE_READ_END 0
#define PIPE_WRITE_END 1
#define CHUNK_SIZE 1024
#define NUM_CHILDREN 3
#define TEST_DATA_SIZE 50000

typedef struct {
    sem_t sem_write;
    sem_t sem_all_write_done;
    sem_t sem_read_done;
    int write_complete;
    int bytes_written;
    int bytes_read;
    int write_block_count;
    int read_block_count;
} shared_data_t;

int test_pipe_capacity() {
    int pipefd[2];
    int capacity = 0;
    char buffer[CHUNK_SIZE];
    int flags;

    memset(buffer, 'A', CHUNK_SIZE);

    if (pipe(pipefd) == -1) {
        perror("管道创建失败");
        return -1;
    }

    printf("测试管道容量\n");

    flags = fcntl(pipefd[PIPE_WRITE_END], F_GETFL);
    fcntl(pipefd[PIPE_WRITE_END], F_SETFL, flags | O_NONBLOCK);

    while (1) {
        int written = write(pipefd[PIPE_WRITE_END], buffer, CHUNK_SIZE);
        if (written > 0) {
            capacity += written;
        } else if (written == -1) {
            if (errno == EAGAIN) {
                printf("管道已满\n");
                printf("管道容量: %d 字节 (%.2f KB)\n", capacity, capacity / 1024.0);
                break;
            } else {
                perror("写入错误");
                break;
            }
        }
    }

    fcntl(pipefd[PIPE_WRITE_END], F_SETFL, flags);

    close(pipefd[PIPE_READ_END]);
    close(pipefd[PIPE_WRITE_END]);

    return capacity;
}

void child_process(int write_fd, int child_id, shared_data_t *shared) {
    srand(time(NULL) ^ (getpid() << 16));
    
    printf("子进程%d[PID:%d] 开始\n", child_id, getpid());
    
    char *test_data = malloc(TEST_DATA_SIZE);
    memset(test_data, 'A' + child_id - 1, TEST_DATA_SIZE);
    snprintf(test_data, 100, "子进程%d的测试数据[%d字节]:", child_id, TEST_DATA_SIZE);
    
    int total_written = 0;
    int write_attempts = 0;
    int is_blocking = 0;
    
    sem_wait(&shared->sem_write);
    
    printf("子进程%d 获得写入权限，开始写入 %d 字节\n", child_id, TEST_DATA_SIZE);
    
    int flags = fcntl(write_fd, F_GETFL);
    
    while (total_written < TEST_DATA_SIZE) {
        int to_write = TEST_DATA_SIZE - total_written;
        if (to_write > 4096) to_write = 4096;
        
        int written = write(write_fd, test_data + total_written, to_write);
        write_attempts++;
        
        if (written > 0) {
            total_written += written;
            shared->bytes_written += written;
            
            if (write_attempts % 5 == 0) {
                printf("子进程%d 已写入 %d/%d 字节\n", child_id, total_written, TEST_DATA_SIZE);
            }
        } else if (written == -1) {
            if (errno == EAGAIN) {
                if (!is_blocking) {
                    printf("子进程%d: 管道满，写入被阻塞！\n", child_id);
                    shared->write_block_count++;
                    is_blocking = 1;
                }
                
                fcntl(write_fd, F_SETFL, flags & ~O_NONBLOCK);
                
                written = write(write_fd, test_data + total_written, to_write);
                
                fcntl(write_fd, F_SETFL, flags | O_NONBLOCK);
                
                if (written > 0) {
                    total_written += written;
                    shared->bytes_written += written;
                    printf("子进程%d: 从阻塞恢复，写入 %d 字节\n", child_id, written);
                    is_blocking = 0;
                }
            } else {
                perror("写入错误");
                break;
            }
        }
        
        usleep(1000);
    }
    
    fcntl(write_fd, F_SETFL, flags);
    free(test_data);
    
    printf("子进程%d 完成写入，总写入 %d 字节，尝试次数: %d\n", child_id, total_written, write_attempts);
    
    sem_post(&shared->sem_write);
    
    __sync_fetch_and_add(&shared->write_complete, 1);
    
    if (shared->write_complete == NUM_CHILDREN) {
        sem_post(&shared->sem_all_write_done);
    }
    
    printf("子进程%d 等待父进程读取完成\n", child_id);
    sem_wait(&shared->sem_read_done);
    
    printf("子进程%d 完成\n", child_id);
    close(write_fd);
    exit(0);
}

void parent_process(int read_fd, shared_data_t *shared) {
    char buffer[CHUNK_SIZE];
    int total_read = 0;
    int read_count = 0;
    
    printf("父进程[PID:%d] 开始工作\n", getpid());
    printf("等待所有子进程完成写入...\n");
    
    sem_wait(&shared->sem_all_write_done);
    
    printf("所有子进程完成写入，开始读取...\n");
    
    int flags = fcntl(read_fd, F_GETFL);
    fcntl(read_fd, F_SETFL, flags | O_NONBLOCK);
    
    int is_blocking = 0;
    int expected_total = NUM_CHILDREN * TEST_DATA_SIZE;
    
    while (total_read < expected_total) {
        int n = read(read_fd, buffer, sizeof(buffer) - 1);
        
        if (n > 0) {
            total_read += n;
            read_count++;
            shared->bytes_read += n;
            is_blocking = 0;
            
            if (read_count % 10 == 0) {
                printf("父进程已读取 %d/%d 字节\n", total_read, expected_total);
            }
        } else if (n == 0) {
            printf("管道读取完毕\n");
            break;
        } else if (n == -1) {
            if (errno == EAGAIN) {
                if (!is_blocking) {
                    printf("管道为空，读取被阻塞！\n");
                    shared->read_block_count++;
                    is_blocking = 1;
                }
                
                fcntl(read_fd, F_SETFL, flags & ~O_NONBLOCK);
                
                n = read(read_fd, buffer, sizeof(buffer) - 1);
                
                fcntl(read_fd, F_SETFL, flags | O_NONBLOCK);
                
                if (n > 0) {
                    buffer[n] = '\0';
                    printf("从阻塞恢复，读取 %d 字节\n", n);
                    total_read += n;
                    read_count++;
                    shared->bytes_read += n;
                    is_blocking = 0;
                } else if (n == 0) {
                    printf("管道读取完毕\n");
                    break;
                }
            } else {
                perror("读取错误");
                break;
            }
        }
    }
    
    fcntl(read_fd, F_SETFL, flags);
    
    for (int i = 0; i < NUM_CHILDREN; i++) {
        sem_post(&shared->sem_read_done);
    }
    
    printf("父进程读取完成统计\n");
    printf("读取次数: %d\n", read_count);
    printf("总读取字节: %d\n", total_read);
    printf("总写入字节: %d\n", shared->bytes_written);
    printf("写入阻塞次数: %d\n", shared->write_block_count);
    printf("读取阻塞次数: %d\n", shared->read_block_count);
    printf("数据丢失: %d 字节\n", shared->bytes_written - total_read);
}

int main() {
    int pipefd[2];
    pid_t pids[NUM_CHILDREN];
    int pipe_capacity;
    
    printf("管道通信实验 \n");
    
    pipe_capacity = test_pipe_capacity();
    if (pipe_capacity <= 0) {
        printf("管道容量测试失败\n");
        return 1;
    }
    
    printf("创建主通信管道\n");
    
    if (pipe(pipefd) == -1) {
        perror("管道创建失败");
        return 1;
    }
    
    printf("管道创建成功，读端=%d，写端=%d\n", pipefd[PIPE_READ_END], pipefd[PIPE_WRITE_END]);
    
    shared_data_t *shared = mmap(NULL, sizeof(shared_data_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared == MAP_FAILED) {
        perror("共享内存创建失败");
        return 1;
    }
    
    memset(shared, 0, sizeof(shared_data_t));
    
    if (sem_init(&shared->sem_write, 1, 1) == -1) {
        perror("写入信号量初始化失败");
        return 1;
    }
    sem_init(&shared->sem_all_write_done, 1, 0);
    sem_init(&shared->sem_read_done, 1, 0);
    
    int flags = fcntl(pipefd[PIPE_WRITE_END], F_GETFL);
    fcntl(pipefd[PIPE_WRITE_END], F_SETFL, flags | O_NONBLOCK);
    
    printf("创建子进程\n");
    for (int i = 0; i < NUM_CHILDREN; i++) {
        pid_t pid = fork();
        
        if (pid < 0) {
            perror("fork失败");
            return 1;
        } else if (pid == 0) {
            close(pipefd[PIPE_READ_END]);
            child_process(pipefd[PIPE_WRITE_END], i + 1, shared);
        } else {
            pids[i] = pid;
            printf("创建子进程%d, PID=%d\n", i + 1, pid);
        }
    }
    
    if (getpid() != 0) {
        close(pipefd[PIPE_WRITE_END]);
        
        parent_process(pipefd[PIPE_READ_END], shared);
        
        printf("等待子进程退出\n");
        for (int i = 0; i < NUM_CHILDREN; i++) {
            int status;
            waitpid(pids[i], &status, 0);
            printf("子进程%d 已退出，状态: %d\n", i + 1, WEXITSTATUS(status));
        }
        
        printf("实验完成\n");
        
        printf("最终统计\n");
        printf("管道容量: %d 字节\n", pipe_capacity);
        printf("总写入字节: %d 字节\n", shared->bytes_written);
        printf("总读取字节: %d 字节\n", shared->bytes_read);
        printf("写入阻塞次数: %d\n", shared->write_block_count);
        printf("读取阻塞次数: %d\n", shared->read_block_count);
        printf("数据完整性: %s\n", 
               (shared->bytes_written == shared->bytes_read) ? "完整" : "有丢失");
        
        sem_destroy(&shared->sem_write);
        sem_destroy(&shared->sem_all_write_done);
        sem_destroy(&shared->sem_read_done);
        munmap(shared, sizeof(shared_data_t));
        
        close(pipefd[PIPE_READ_END]);
    }
    
    return 0;
}
