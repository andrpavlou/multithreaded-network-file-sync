#ifndef SYNC_TASK_H
#define SYNC_TASK_H

#include "sync_info.h"
#include "nfs_manager_def.h"

typedef struct {
    char filename[BUFFSIZ];   
    manager_command manager_cmd;       
    sync_info_mem_store *node;
} sync_task;


typedef struct {
    int head;
    int tail;

    unsigned int size;
    unsigned int buffer_slots;

    pthread_mutex_t mutex;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;

    sync_task **tasks_array;
} sync_task_ts;

//////// THREAD SAFE QUEUE //////

int init_sync_task_ts(sync_task_ts *queue, const int buffer_slots);
void enqueue_task(sync_task_ts *queue, sync_task *newtask);
sync_task* dequeue_task(sync_task_ts *queue);
void free_queue_task(sync_task_ts *queue);

/////////////////////////////////

    
#endif
