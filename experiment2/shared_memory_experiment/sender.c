// sender.c - 发送者程序
#include "common.h"

int main() {
    int shm_id, sem_id;
    shared_data_t *shared_mem = NULL;
    
    printf("========== SENDER程序 ==========\n");
    printf("PID: %d\n", getpid());
    
    // 步骤1: 创建或获取信号量
    printf("\n1. 创建信号量...\n");
    sem_id = create_semaphore(SEM_KEY, 3);
    
    // 步骤2: 创建共享内存
    printf("\n2. 创建共享内存...\n");
    shm_id = shmget(SHM_KEY, sizeof(shared_data_t), IPC_CREAT | 0666);
    if (shm_id == -1) {
        perror("共享内存创建失败");
        remove_semaphore(sem_id);
        exit(1);
    }
    printf("共享内存创建成功，ID: %d\n", shm_id);
    
    // 步骤3: 将共享内存映射到进程地址空间
    printf("\n3. 映射共享内存...\n");
    shared_mem = (shared_data_t *)shmat(shm_id, NULL, 0);
    if (shared_mem == (void *)-1) {
        perror("共享内存映射失败");
        shmctl(shm_id, IPC_RMID, NULL);
        remove_semaphore(sem_id);
        exit(1);
    }
    printf("共享内存映射成功，地址: %p\n", shared_mem);
    
    // 步骤4: 初始化共享内存
    printf("\n4. 初始化共享内存...\n");
    memset(shared_mem, 0, sizeof(shared_data_t));
    shared_mem->sender_pid = getpid();
    shared_mem->is_written = 0;
    shared_mem->is_responded = 0;
    printf("共享内存初始化完成\n");
    
    // 步骤5: 等待用户输入
    printf("\n5. 等待用户输入消息...\n");
    printf("请输入要发送的消息: ");
    
    char input_buffer[1024];
    if (fgets(input_buffer, sizeof(input_buffer), stdin) == NULL) {
        printf("输入错误或EOF\n");
        goto cleanup;
    }
    
    // 移除末尾的换行符
    input_buffer[strcspn(input_buffer, "\n")] = '\0';
    
    printf("输入的消息: %s\n", input_buffer);
    printf("消息长度: %ld 字节\n", strlen(input_buffer));
    
    // 步骤6: 写入共享内存（使用信号量同步）
    printf("\n6. 向共享内存写入消息...\n");
    
    // 获取写权限
    printf("等待写权限...\n");
    P(sem_id, SEM_WRITE);
    printf("获得写权限\n");
    
    // 获取互斥锁
    P(sem_id, SEM_MUTEX);
    printf("获得互斥锁\n");
    
    // 写入消息
    strncpy(shared_mem->message, input_buffer, sizeof(shared_mem->message) - 1);
    shared_mem->is_written = 1;
    shared_mem->is_responded = 0;
    
    printf("消息已写入共享内存\n");
    printf("写入时间: %ld\n", time(NULL));
    
    // 释放互斥锁
    V(sem_id, SEM_MUTEX);
    printf("释放互斥锁\n");
    
    // 通知receiver可以读取
    V(sem_id, SEM_READ);
    printf("通知receiver读取\n");
    
    // 步骤7: 等待receiver应答
    printf("\n7. 等待receiver应答...\n");
    
    int timeout = 30;  // 30秒超时
    int waited = 0;
    
    while (waited < timeout) {
        // 检查是否收到应答
        P(sem_id, SEM_MUTEX);
        if (shared_mem->is_responded) {
            V(sem_id, SEM_MUTEX);
            break;
        }
        V(sem_id, SEM_MUTEX);
        
        printf("等待应答... 已等待 %d 秒\n", waited);
        sleep(1);
        waited++;
    }
    
    if (waited >= timeout) {
        printf("等待超时，未收到应答\n");
    } else {
        // 步骤8: 显示应答消息
        printf("\n8. 收到receiver应答\n");
        
        P(sem_id, SEM_MUTEX);
        printf("应答消息: %s\n", shared_mem->message);
        printf("应答时间: %ld\n", time(NULL));
        printf("Receiver PID: %d\n", shared_mem->receiver_pid);
        V(sem_id, SEM_MUTEX);
    }
    
    // 步骤9: 清理资源
    printf("\n9. 清理资源...\n");
    
cleanup:
    // 取消共享内存映射
    if (shared_mem != NULL && shared_mem != (void *)-1) {
        if (shmdt(shared_mem) == -1) {
            perror("取消共享内存映射失败");
        } else {
            printf("已取消共享内存映射\n");
        }
    }
    
    // 删除共享内存
    if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
        perror("删除共享内存失败");
    } else {
        printf("共享内存已删除\n");
    }
    
    // 删除信号量
    remove_semaphore(sem_id);
    
    printf("\n========== SENDER程序结束 ==========\n");
    return 0;
}
