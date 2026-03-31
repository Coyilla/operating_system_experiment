// msg_queue_experiment.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <semaphore.h>

// 消息结构体
typedef struct {
    long mtype;            // 消息类型
    char mtext[256];       // 消息内容
} msg_buf;

// 全局变量
int msgid;                 // 消息队列ID
key_t msg_key;             // 消息队列键值
sem_t sem_sender1;         // sender1信号量
sem_t sem_sender2;         // sender2信号量
sem_t sem_receiver;        // receiver信号量
pthread_mutex_t input_mutex;  // 输入互斥锁

// 函数声明
void* sender1(void* arg);
void* sender2(void* arg);
void* receiver(void* arg);
int create_message_queue();
void cleanup();

// 主函数
int main() {
    pthread_t tid1, tid2, tid3;
    int ret;
    
    printf("========================================\n");
    printf("   消息队列线程通信实验\n");
    printf("   三个线程通过消息队列通信\n");
    printf("========================================\n\n");
    
    // 1. 创建消息队列
    msg_key = ftok("/tmp", 'A');
    if (msg_key == -1) {
        perror("ftok失败");
        exit(1);
    }
    
    msgid = msgget(msg_key, IPC_CREAT | 0666);
    if (msgid == -1) {
        perror("消息队列创建失败");
        exit(1);
    }
    printf("消息队列创建成功，ID: %d\n\n", msgid);
    
    // 2. 初始化信号量和互斥锁
    sem_init(&sem_sender1, 0, 1);     // sender1先运行
    sem_init(&sem_sender2, 0, 1);     // sender2先运行
    sem_init(&sem_receiver, 0, 0);    // receiver等待
    pthread_mutex_init(&input_mutex, NULL);
    
    // 3. 创建三个线程
    printf("创建线程...\n");
    
    ret = pthread_create(&tid1, NULL, sender1, NULL);
    if (ret != 0) {
        perror("sender1线程创建失败");
        cleanup();
        exit(1);
    }
    printf("sender1线程创建成功\n");
    
    ret = pthread_create(&tid2, NULL, sender2, NULL);
    if (ret != 0) {
        perror("sender2线程创建失败");
        cleanup();
        exit(1);
    }
    printf("sender2线程创建成功\n");
    
    ret = pthread_create(&tid3, NULL, receiver, NULL);
    if (ret != 0) {
        perror("receiver线程创建失败");
        cleanup();
        exit(1);
    }
    printf("receiver线程创建成功\n\n");
    
    // 4. 等待所有线程结束
    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);
    pthread_join(tid3, NULL);
    
    // 5. 清理资源
    cleanup();
    
    printf("\n=== 实验完成 ===\n");
    return 0;
}

// 清理函数
void cleanup() {
    // 销毁信号量
    sem_destroy(&sem_sender1);
    sem_destroy(&sem_sender2);
    sem_destroy(&sem_receiver);
    
    // 销毁互斥锁
    pthread_mutex_destroy(&input_mutex);
    
    // 删除消息队列
    if (msgctl(msgid, IPC_RMID, NULL) == -1) {
        perror("消息队列删除失败");
    } else {
        printf("消息队列已删除\n");
    }
}

// sender1线程函数
void* sender1(void* arg) {
    msg_buf msg;
    char input[256];
    int running = 1;
    
    printf("sender1线程启动 (PID: %d, TID: %lu)\n", 
           getpid(), pthread_self());
    
    // 设置消息类型为1（sender1的消息）
    msg.mtype = 1;
    
    while (running) {
        // 获取输入互斥锁，避免多个线程同时读取输入
        pthread_mutex_lock(&input_mutex);
        printf("\n[sender1] 请输入消息 (输入'exit'结束): ");
        fgets(input, sizeof(input), stdin);
        pthread_mutex_unlock(&input_mutex);
        
        // 去掉换行符
        input[strcspn(input, "\n")] = '\0';
        
        if (strcmp(input, "exit") == 0) {
            // 发送结束消息
            strcpy(msg.mtext, "end1");
            if (msgsnd(msgid, &msg, strlen(msg.mtext) + 1, 0) == -1) {
                perror("sender1发送end1失败");
            } else {
                printf("[sender1] 发送结束消息: end1\n");
            }
            running = 0;
        } else {
            // 发送普通消息
            strcpy(msg.mtext, input);
            if (msgsnd(msgid, &msg, strlen(msg.mtext) + 1, 0) == -1) {
                perror("sender1发送消息失败");
            } else {
                printf("[sender1] 发送消息: %s\n", input);
            }
        }
        
        usleep(100000);  // 短暂延迟，避免输出混乱
    }
    
    // 等待receiver的应答
    printf("[sender1] 等待receiver应答...\n");
    
    // 接收类型为101的应答消息
    if (msgrcv(msgid, &msg, sizeof(msg.mtext), 101, 0) == -1) {
        perror("sender1接收应答失败");
    } else {
        printf("[sender1] 收到应答: %s\n", msg.mtext);
    }
    
    printf("[sender1] 线程结束\n");
    return NULL;
}

// sender2线程函数
void* sender2(void* arg) {
    msg_buf msg;
    char input[256];
    int running = 1;
    
    printf("sender2线程启动 (PID: %d, TID: %lu)\n", 
           getpid(), pthread_self());
    
    // 设置消息类型为2（sender2的消息）
    msg.mtype = 2;
    
    while (running) {
        // 获取输入互斥锁
        pthread_mutex_lock(&input_mutex);
        printf("\n[sender2] 请输入消息 (输入'exit'结束): ");
        fgets(input, sizeof(input), stdin);
        pthread_mutex_unlock(&input_mutex);
        
        // 去掉换行符
        input[strcspn(input, "\n")] = '\0';
        
        if (strcmp(input, "exit") == 0) {
            // 发送结束消息
            strcpy(msg.mtext, "end2");
            if (msgsnd(msgid, &msg, strlen(msg.mtext) + 1, 0) == -1) {
                perror("sender2发送end2失败");
            } else {
                printf("[sender2] 发送结束消息: end2\n");
            }
            running = 0;
        } else {
            // 发送普通消息
            strcpy(msg.mtext, input);
            if (msgsnd(msgid, &msg, strlen(msg.mtext) + 1, 0) == -1) {
                perror("sender2发送消息失败");
            } else {
                printf("[sender2] 发送消息: %s\n", input);
            }
        }
        
        usleep(100000);  // 短暂延迟
    }
    
    // 等待receiver的应答
    printf("[sender2] 等待receiver应答...\n");
    
    // 接收类型为102的应答消息
    if (msgrcv(msgid, &msg, sizeof(msg.mtext), 102, 0) == -1) {
        perror("sender2接收应答失败");
    } else {
        printf("[sender2] 收到应答: %s\n", msg.mtext);
    }
    
    printf("[sender2] 线程结束\n");
    return NULL;
}

// receiver线程函数
void* receiver(void* arg) {
    msg_buf msg;
    int end1_received = 0;
    int end2_received = 0;
    
    printf("receiver线程启动 (PID: %d, TID: %lu)\n", 
           getpid(), pthread_self());
    printf("[receiver] 等待接收消息...\n\n");
    
    while (!(end1_received && end2_received)) {
        // 接收消息（阻塞等待）
        // msgtype=0 表示接收所有类型的消息
        if (msgrcv(msgid, &msg, sizeof(msg.mtext), 0, 0) == -1) {
            perror("receiver接收消息失败");
            break;
        }
        
        printf("[receiver] 收到消息 (类型%ld): %s\n", msg.mtype, msg.mtext);
        
        // 检查是否是结束消息
        if (msg.mtype == 1 && strcmp(msg.mtext, "end1") == 0) {
            end1_received = 1;
            printf("[receiver] 收到sender1的结束消息\n");
            
            // 向sender1发送应答
            msg.mtype = 101;  // 给sender1的应答类型
            strcpy(msg.mtext, "over1");
            if (msgsnd(msgid, &msg, strlen(msg.mtext) + 1, 0) == -1) {
                perror("receiver发送over1失败");
            } else {
                printf("[receiver] 发送应答over1给sender1\n");
            }
        } 
        else if (msg.mtype == 2 && strcmp(msg.mtext, "end2") == 0) {
            end2_received = 1;
            printf("[receiver] 收到sender2的结束消息\n");
            
            // 向sender2发送应答
            msg.mtype = 102;  // 给sender2的应答类型
            strcpy(msg.mtext, "over2");
            if (msgsnd(msgid, &msg, strlen(msg.mtext) + 1, 0) == -1) {
                perror("receiver发送over2失败");
            } else {
                printf("[receiver] 发送应答over2给sender2\n");
            }
        }
        
        usleep(50000);  // 短暂延迟
    }
    
    printf("[receiver] 收到所有结束消息\n");
    printf("[receiver] 线程结束\n");
    
    return NULL;
}
