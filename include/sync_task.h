#ifndef SYNC_TASK_H
#define SYNC_TASK_H

#include "sync_info.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define MAX_QUEUE_SIZE 5


typedef struct {
    char filename[BUFFSIZ];             
    // client_operation op;
    sync_info_mem_store *node;
    struct sync_task *next;
} sync_task;

typedef struct {
    sync_task *tasks_array[MAX_QUEUE_SIZE];
    int head;
    int tail;
    int size;

    pthread_mutex_t mutex;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;

} sync_task_ts;

//////// THREAD SAFE QUEUE //////
void init_sync_task_ts(sync_task_ts *queue);
void enqueue_task(sync_task_ts *queue, sync_task *newtask);
sync_task* dequeue_task(sync_task_ts *queue);

/////////////////////////////////


// void enqueue_sync_task(sync_task **head, sync_task **tail, sync_task *new_task);
// sync_task* dequeue_sync_task(sync_task **head, sync_task **tail);
// sync_task* dequeue_full_sync_task(sync_task **head, sync_task **tail);
// int check_if_can_queue(sync_task *head, const char* filename, operation current_op);
// void dequeue_pending_task(sync_task **pending_task_queue_head, sync_task **pending_task_queue_tail,
//     sync_task **task_queue_head, sync_task **task_queue_tail);
    
#endif
