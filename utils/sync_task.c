#include "sync_task.h"

// Initialization of thread safe sync task queue
int init_sync_task_ts(sync_task_ts *queue, const int buffer_slots){
    queue->head     = 0;
    queue->tail     = 0;
    queue->size     = 0;

    queue->buffer_slots = buffer_slots;
    queue->tasks_array = malloc(buffer_slots * sizeof(sync_task*));
    
    if(queue->tasks_array == NULL){
        perror("malloc");
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

    queue->tasks_array[queue->tail] = newtask;
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


void free_queue_task(sync_task_ts *queue){
    free(queue->tasks_array);
}
