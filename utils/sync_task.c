#include "sync_task.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Initialization of thread safe sync task queue
int init_sync_task_ts(sync_task_ts *queue, const int buffer_slots){
    queue->head     = 0;
    queue->tail     = 0;
    queue->size     = 0;
    
    atomic_init(&queue->cancel_cmd_counter, 0);

    queue->buffer_slots = buffer_slots;
    queue->tasks_array = malloc(buffer_slots * sizeof(sync_task*));
    
    if(queue->tasks_array == NULL){
        #ifdef DEBUG
        perror("malloc");
        #endif
        return 1;
    }

    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
    pthread_cond_init(&queue->not_full, NULL);

    return 0;
}

// Thread safe enqueue
void enqueue_task(sync_task_ts *queue, sync_task *newtask){
    pthread_mutex_lock(&queue->mutex);

    while(queue->size == queue->buffer_slots){
        pthread_cond_wait(&queue->not_full, &queue->mutex);
    }
    
    unsigned int insert_idx = newtask->manager_cmd.op == CANCEL ? queue->head : queue->tail;

    // CANCEL - Operation -> Priority insert
    if(newtask->manager_cmd.op == CANCEL){
        /* Starting from the (end of the queue) to start, we move tasks one index backwards, 
           shifting all the elements to the right and then insert the current task at the head. */
        for(int i = queue->size - 1; i >= 0; i--){
            int old_idx = (queue->head + i)     % queue->buffer_slots;
            int new_idx = (queue->head + i + 1) % queue->buffer_slots;

            queue->tasks_array[new_idx] = queue->tasks_array[old_idx];
        }
        atomic_fetch_add(&queue->cancel_cmd_counter, 1);
    }


    queue->tasks_array[insert_idx] = newtask;
    queue->tail = (queue->tail + 1) % queue->buffer_slots;
    queue->size++;
    
    

    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex);
}

// Thread safe dequeue
sync_task* dequeue_task(sync_task_ts *queue){
    pthread_mutex_lock(&queue->mutex);
    
    while(!queue->size){
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }
    
    sync_task *task = queue->tasks_array[queue->head];
    
    queue->head = (queue->head + 1) % queue->buffer_slots;
    queue->size--;
    
    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);
    
    return task;
}

bool task_exists_add(sync_task_ts *queue, const char* filename, manager_command curr_cmd){
    pthread_mutex_lock(&queue->mutex);

    for(int i = 0; i < queue->size; i++){
        int index = (queue->head + i) % queue->buffer_slots;
        sync_task *task = queue->tasks_array[index];
        

        if(IS_DUPLICATE_ADD_TASK(task, filename, curr_cmd)){ 
            pthread_mutex_unlock(&queue->mutex);
            return TRUE; 
        }

    }

    pthread_mutex_unlock(&queue->mutex);
    return FALSE;
}


bool task_exists_cancel(sync_task_ts *queue, const char* ip, const char* cancel_dir, const int port){
    pthread_mutex_lock(&queue->mutex);

    for(int i = 0; i < queue->size; i++){
        int index = (queue->head + i) % queue->buffer_slots;
        sync_task *task = queue->tasks_array[index];
        
        if(IS_DUPLICATE_CANCEL_TASK(task, ip, cancel_dir, port)){ 
            pthread_mutex_unlock(&queue->mutex);
            return TRUE; 
        }
    }
    pthread_mutex_unlock(&queue->mutex);
    return FALSE;
}

void free_queue_task(sync_task_ts *queue){
    free(queue->tasks_array);
}
