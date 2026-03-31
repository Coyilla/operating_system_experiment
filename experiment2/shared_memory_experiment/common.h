// common.h - 共享定义
#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#define SHM_KEY 0x1234
#define SEM_KEY 0x5678

#define SHM_SIZE 1024

typedef struct {
    char message[1024];
    int  is_written;
    int  is_responded;
    pid_t sender_pid;
    pid_t receiver_pid;
} shared_data_t;

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

#define SEM_MUTEX 0
#define SEM_WRITE 1
#define SEM_READ  2


#define P(sem_id, sem_num) semaphore_operation(sem_id, sem_num, -1)
#define V(sem_id, sem_num) semaphore_operation(sem_id, sem_num, 1)

int create_semaphore(int key, int num_sems);
void semaphore_operation(int sem_id, int sem_num, int op);
void remove_semaphore(int sem_id);
void remove_shared_memory(int shm_id);

#endif
