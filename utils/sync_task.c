#include "sync_task.h"

// Initialization of thread safe sync task queue
void init_sync_task_ts(sync_task_ts *queue){
    queue->head     = 0;
    queue->tail     = 0;
    queue->size     = 0;

    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
    pthread_cond_init(&queue->not_full, NULL);
}

// Thread safe enqueue
void enqueue_task(sync_task_ts *queue, sync_task *newtask){
    pthread_mutex_lock(&queue->mutex);

    while(queue->size == MAX_QUEUE_SIZE){
        pthread_cond_wait(&queue->not_full, &queue->mutex);
    }

    queue->tasks_array[queue->tail] = newtask;
    queue->tail = (queue->tail + 1) % MAX_QUEUE_SIZE;
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
    queue->head = (queue->head + 1) % MAX_QUEUE_SIZE;
    queue->size--;

    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);

    return task;
}



///////////////// HW1 Might no need for this one /////////////////
///////
// insert task at the end of the queue.
// void enqueue_sync_task(sync_task **head, sync_task **tail, sync_task *new_task) {
//     new_task->next = NULL;
//     // empty so head = tail 
//     if(*tail == NULL){
//         *head = *tail = new_task;
//         return;
//     }
    
//     (*tail)->next = new_task;
//     *tail = new_task;
// }

// // remove first task of the queue.
// sync_task* dequeue_sync_task(sync_task **head, sync_task **tail) {
//     if(*head == NULL) return NULL;

//     sync_task *task = *head;
//     *head = (*head)->next;

//     if(*head == NULL) 
//         *tail = NULL;
    
//     return task;
// }


// sync_task* dequeue_full_sync_task(sync_task **head, sync_task **tail) {
//     if(!head || !*head) return NULL;

//     sync_task *prev = NULL;
//     sync_task *curr = *head;

//     // find full operation
//     while(curr != NULL && curr->op != FULL){
//         prev = curr;
//         curr = curr->next;
//     }

//     // not found
//     if(curr == NULL) return NULL;

//     // removing the head
//     if(prev == NULL){
//         *head = curr->next;
        
//         if(*tail == curr) *tail = NULL;
        
//         curr->next = NULL;
//         return curr;
//     } 
    
//     prev->next = curr->next;
//     if(*tail == curr) *tail = prev; 
    
        
//     curr->next = NULL;
//     return curr;
// }


// /* 
//  * functions mainly for safety when using echo "..." >> file 
//  */

// int check_if_can_queue(sync_task *head, const char* filename, operation current_op){
//     sync_task *iter_node = head;
    
//     while(iter_node != NULL){
//         if(!strcmp(iter_node->filename, filename) && iter_node->op == ADDED && (current_op == MODIFIED || current_op == DELETED)){
//             return 1;
//         }
//         iter_node = iter_node->next;
//     }
//     return 0;
// }


// void dequeue_pending_task(sync_task **pending_task_queue_head, sync_task **pending_task_queue_tail,
//     sync_task **task_queue_head, sync_task **task_queue_tail){
    
//     if(*pending_task_queue_head == NULL) return;
//     // Try to requeue pending tasks
//     sync_task *prev_pending = NULL;
//     sync_task *iter_pending = *pending_task_queue_head;

//     while(iter_pending != NULL){
//         sync_info_mem_store *find = find_sync_info(iter_pending->node, iter_pending->node->source);
//         if(find->worker_pid == -1 && check_if_can_queue(*task_queue_head, iter_pending->filename, iter_pending->op) == 0){


//             // removing task in the middle or last, update prev
//             if(prev_pending != NULL){
//                 prev_pending->next = iter_pending->next;
//             } else {
//                 // removing the head, so update the queue head
//                 *pending_task_queue_head = iter_pending->next;
//             }
            
//             // removing tail, so update tail with one node back
//             if(*pending_task_queue_tail == iter_pending){
//                 *pending_task_queue_tail = prev_pending;
//             }

//             sync_task *to_enqueue = iter_pending;
//             iter_pending = iter_pending->next; 

//             to_enqueue->next = NULL;
//             enqueue_sync_task(task_queue_head, task_queue_tail, to_enqueue);
//             // printf(">>>ENQUEUE\n");
//             continue;
//         }

//         // move to the next node, without enqueue_sync_task
//         prev_pending = iter_pending;
//         iter_pending = iter_pending->next;
//     }
// }