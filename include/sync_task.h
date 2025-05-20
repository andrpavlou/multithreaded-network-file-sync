#ifndef SYNC_TASK_H
#define SYNC_TASK_H

#include "sync_info.h"
#include <stdlib.h>
#include <string.h>

// typedef enum { 
//     ADDED, MODIFIED, DELETED, FULL
// } operation;

typedef struct sync_task {
    char filename[BUFFSIZ];             
    operation op;
    sync_info_mem_store *node;
    struct sync_task *next;
} sync_task;

void enqueue_sync_task(sync_task **head, sync_task **tail, sync_task *new_task);
sync_task* dequeue_sync_task(sync_task **head, sync_task **tail);
sync_task* dequeue_full_sync_task(sync_task **head, sync_task **tail);
int check_if_can_queue(sync_task *head, const char* filename, operation current_op);
void dequeue_pending_task(sync_task **pending_task_queue_head, sync_task **pending_task_queue_tail,
    sync_task **task_queue_head, sync_task **task_queue_tail);
    
#endif
