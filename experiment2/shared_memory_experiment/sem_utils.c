// sem_utils.c - 信号量工具函数
#include "common.h"

int create_semaphore(int key, int num_sems) {
    int sem_id = semget(key, num_sems, IPC_CREAT | 0666);
    if (sem_id == -1) {
        perror("信号量创建/获取失败");
        exit(1);
    }
    
    union semun arg;
    
    arg.val = 1;
    if (semctl(sem_id, SEM_MUTEX, SETVAL, arg) == -1) {
        perror("初始化互斥信号量失败");
        exit(1);
    }
    
    arg.val = 1;
    if (semctl(sem_id, SEM_WRITE, SETVAL, arg) == -1) {
        perror("初始化写信号量失败");
        exit(1);
    }
    
    arg.val = 0;
    if (semctl(sem_id, SEM_READ, SETVAL, arg) == -1) {
        perror("初始化读信号量失败");
        exit(1);
    }
    
    printf("信号量创建成功，ID: %d\n", sem_id);
    return sem_id;
}

void semaphore_operation(int sem_id, int sem_num, int op) {
    struct sembuf sb;
    sb.sem_num = sem_num;   
    sb.sem_op = op;         
    sb.sem_flg = 0;          
    
    if (semop(sem_id, &sb, 1) == -1) {
        perror("信号量操作失败");
        exit(1);
    }
}

void remove_semaphore(int sem_id) {
    if (semctl(sem_id, 0, IPC_RMID) == -1) {
        perror("删除信号量失败");
    } else {
        printf("信号量已删除\n");
    }
}
