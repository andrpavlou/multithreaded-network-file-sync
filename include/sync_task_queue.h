#ifndef SYNC_TASK_H
#define SYNC_TASK_H

#include "sync_info_list.h"
#include "nfs_manager_def.h"



#define IS_DUPLICATE_ADD_TASK(task, filename, add_cmd)(                       \
            (!strcmp((task)->filename, (filename))                         && \
            !strcmp((task)->manager_cmd.source_ip, (add_cmd).source_ip)    && \
            !strcmp((task)->manager_cmd.target_ip, (add_cmd).target_ip)    && \
            !strcmp((task)->manager_cmd.source_dir, (add_cmd).source_dir)  && \
            !strcmp((task)->manager_cmd.target_dir, (add_cmd).target_dir)  && \
            (task)->manager_cmd.source_port == (add_cmd).source_port       && \
            (task)->manager_cmd.target_port == (add_cmd).target_port))


#define IS_DUPLICATE_CANCEL_TASK(task, ip, cancel_dir, port)(          \
            (!strcmp((task)->manager_cmd.source_ip, (ip))           && \
            !strcmp((task)->manager_cmd.cancel_dir, (cancel_dir))   && \
            (task)->manager_cmd.source_port == (port)))




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

    atomic_int cancel_cmd_counter;

    sync_task **tasks_array;
} sync_task_ts;

//////// THREAD SAFE QUEUE //////

int init_sync_task_ts(sync_task_ts *queue, const int buffer_slots);

void enqueue_task(sync_task_ts *queue, sync_task *newtask);
sync_task* dequeue_task(sync_task_ts *queue);


bool task_exists_add(sync_task_ts *queue, const char* file, manager_command curr_cmd);
bool task_exists_cancel(sync_task_ts *queue, const char* ip, const char* cancel_dir, const int port);

void free_queue_task(sync_task_ts *queue);

/////////////////////////////////

    
#endif
