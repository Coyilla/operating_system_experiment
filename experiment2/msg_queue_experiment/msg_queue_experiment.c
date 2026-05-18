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

typedef struct {
    long mtype;            
    char mtext[256];      
} msg_buf;

int msgid;                
key_t msg_key;     
sem_t sem_sender1;         
sem_t sem_sender2;        
sem_t sem_receiver;       
pthread_mutex_t input_mutex; 

void* sender1(void* arg);
void* sender2(void* arg);
void* receiver(void* arg);
int create_message_queue();
void cleanup();

int main() {
    pthread_t tid1, tid2, tid3;
    int ret;
    
    printf("\n");
    printf("消息队列线程通信实验\n");
    printf("三个线程通过消息队列通信\n");
    printf("\n\n");
    
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
    
    sem_init(&sem_sender1, 0, 1);    
    sem_init(&sem_sender2, 0, 1);  
    sem_init(&sem_receiver, 0, 0);    
    pthread_mutex_init(&input_mutex, NULL);
    
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
    
    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);
    pthread_join(tid3, NULL);
    
    cleanup();
    
    printf("\n实验完成\n");
    return 0;
}

void cleanup() {
    sem_destroy(&sem_sender1);
    sem_destroy(&sem_sender2);
    sem_destroy(&sem_receiver);
    
    pthread_mutex_destroy(&input_mutex);
    
    if (msgctl(msgid, IPC_RMID, NULL) == -1) {
        perror("消息队列删除失败");
    } else {
        printf("消息队列已删除\n");
    }
}

void* sender1(void* arg) {
    msg_buf msg;
    char input[256];
    int running = 1;
    
    printf("sender1线程启动 (PID: %d, TID: %lu)\n", 
           getpid(), pthread_self());
    
    msg.mtype = 1;
    
    while (running) {
        pthread_mutex_lock(&input_mutex);
        printf("\n[sender1] 请输入消息 (输入'exit'结束): \n");
        fgets(input, sizeof(input), stdin);
        pthread_mutex_unlock(&input_mutex);
        
        input[strcspn(input, "\n")] = '\0';
        
        if (strcmp(input, "exit") == 0) {
            strcpy(msg.mtext, "end1");
            if (msgsnd(msgid, &msg, strlen(msg.mtext) + 1, 0) == -1) {
                perror("sender1发送end1失败");
            } else {
                printf("[sender1] 发送结束消息: end1\n");
            }
            running = 0;
        } else {
            strcpy(msg.mtext, input);
            if (msgsnd(msgid, &msg, strlen(msg.mtext) + 1, 0) == -1) {
                perror("sender1发送消息失败");
            } else {
                printf("[sender1] 发送消息: %s\n", input);
            }
        }
        
        usleep(100000); 
    }
    
    printf("[sender1] 等待receiver应答...\n");
    
    if (msgrcv(msgid, &msg, sizeof(msg.mtext), 101, 0) == -1) {
        perror("sender1接收应答失败");
    } else {
        printf("[sender1] 收到应答: %s\n", msg.mtext);
    }
    
    printf("[sender1] 线程结束\n");
    return NULL;
}

void* sender2(void* arg) {
    msg_buf msg;
    char input[256];
    int running = 1;
    
    printf("sender2线程启动 (PID: %d, TID: %lu)\n", 
           getpid(), pthread_self());
    
    msg.mtype = 2;
    
    while (running) {
        pthread_mutex_lock(&input_mutex);
        printf("\n[sender2] 请输入消息 (输入'exit'结束): \n");
        fgets(input, sizeof(input), stdin);
        pthread_mutex_unlock(&input_mutex);
        
        input[strcspn(input, "\n")] = '\0';
        
        if (strcmp(input, "exit") == 0) {
            strcpy(msg.mtext, "end2");
            if (msgsnd(msgid, &msg, strlen(msg.mtext) + 1, 0) == -1) {
                perror("sender2发送end2失败");
            } else {
                printf("[sender2] 发送结束消息: end2\n");
            }
            running = 0;
        } else {
            strcpy(msg.mtext, input);
            if (msgsnd(msgid, &msg, strlen(msg.mtext) + 1, 0) == -1) {
                perror("sender2发送消息失败");
            } else {
                printf("[sender2] 发送消息: %s\n", input);
            }
        }
        
        usleep(100000); 
    }
    
    printf("[sender2] 等待receiver应答...\n");
    
    if (msgrcv(msgid, &msg, sizeof(msg.mtext), 102, 0) == -1) {
        perror("sender2接收应答失败");
    } else {
        printf("[sender2] 收到应答: %s\n", msg.mtext);
    }
    
    printf("[sender2] 线程结束\n");
    return NULL;
}

void* receiver(void* arg) {
    msg_buf msg;
    int end1_received = 0;
    int end2_received = 0;
    
    printf("receiver线程启动 (PID: %d, TID: %lu)\n", getpid(), pthread_self());
    printf("[receiver] 等待接收消息...\n\n");
    
    while (!(end1_received && end2_received)) {
        if (msgrcv(msgid, &msg, sizeof(msg.mtext), -3, 0) == -1) {
            perror("receiver接收消息失败");
            break;
        }
        
        printf("[receiver] 收到消息 (类型%ld): %s\n", msg.mtype, msg.mtext);
        
        if (msg.mtype == 1 && strcmp(msg.mtext, "end1") == 0) {
            end1_received = 1;
            printf("[receiver] 收到sender1的结束消息\n");
            
            msg.mtype = 101;
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
            
            msg.mtype = 102; 
            strcpy(msg.mtext, "over2");
            if (msgsnd(msgid, &msg, strlen(msg.mtext) + 1, 0) == -1) {
                perror("receiver发送over2失败");
            } else {
                printf("[receiver] 发送应答over2给sender2\n");
            }
        }
        
        usleep(50000); 
    }
    
    printf("[receiver] 收到所有结束消息\n");
    printf("[receiver] 线程结束\n");
    
    return NULL;
}
