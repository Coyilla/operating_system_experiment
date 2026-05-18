// receiver.c - 接收者程序
#include "common.h"

int main() {
    int shm_id, sem_id;
    shared_data_t *shared_mem = NULL;
    
    printf("========== RECEIVER程序 ==========\n");
    printf("PID: %d\n", getpid());
    
    printf("\n1. 获取信号量...\n");
    int retry_count = 0;
    const int max_retries = 5;
    
    while (retry_count < max_retries) {
        sem_id = semget(SEM_KEY, 3, 0666);
        if (sem_id != -1) {
            break;
        }
        
        retry_count++;
        printf("等待信号量... 尝试 %d/%d\n", retry_count, max_retries);
        sleep(1);
    }
    
    if (sem_id == -1) {
        printf("获取信号量失败，请先运行sender程序\n");
        exit(1);
    }
    printf("获取信号量成功，ID: %d\n", sem_id);
    
    printf("\n2. 获取共享内存...\n");
    shm_id = shmget(SHM_KEY, sizeof(shared_data_t), 0666);
    if (shm_id == -1) {
        perror("获取共享内存失败");
        exit(1);
    }
    printf("获取共享内存成功，ID: %d\n", shm_id);
    
    printf("\n3. 映射共享内存...\n");
    shared_mem = (shared_data_t *)shmat(shm_id, NULL, 0);
    if (shared_mem == (void *)-1) {
        perror("共享内存映射失败");
        exit(1);
    }
    printf("共享内存映射成功，地址: %p\n", shared_mem);
    
    printf("\n4. 等待sender消息...\n");
    printf("正在等待消息...\n");
    
    P(sem_id, SEM_READ);
    printf("收到可读信号\n");
    
    P(sem_id, SEM_MUTEX);
    printf("获得互斥锁\n");
    
    printf("\n5. 收到sender消息:\n");
    printf("消息内容: %s\n", shared_mem->message);
    printf("消息长度: %ld 字节\n", strlen(shared_mem->message));
    printf("Sender PID: %d\n", shared_mem->sender_pid);
    printf("接收时间: %ld\n", time(NULL));
    
    shared_mem->receiver_pid = getpid();
    
    V(sem_id, SEM_MUTEX);
    printf("释放互斥锁\n");
    
    printf("\n6. 发送应答消息...\n");
    
    P(sem_id, SEM_MUTEX);
    printf("获得互斥锁\n");
    
    char *response = "over";
    strncpy(shared_mem->message, response, sizeof(shared_mem->message) - 1);
    shared_mem->is_responded = 1;
    
    printf("应答消息: %s\n", shared_mem->message);
    printf("应答时间: %ld\n", time(NULL));
    
    V(sem_id, SEM_MUTEX);
    printf("释放互斥锁\n");
    
    V(sem_id, SEM_WRITE);
    printf("通知sender\n");
    
    printf("\n7. 清理资源...\n");
    
    if (shmdt(shared_mem) == -1) {
        perror("取消共享内存映射失败");
    } else {
        printf("已取消共享内存映射\n");
    }
    
    printf("\n========== RECEIVER程序结束 ==========\n");
    return 0;
}
