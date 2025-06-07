#ifndef MANAGER_WORKER_POOL_H
#define MANAGER_WORKER_POOL_H

#include <signal.h> 
#include "sync_task_queue.h"

extern volatile sig_atomic_t manager_active;


#define CLEANUP(sock_target_push, sock_source_read, curr_task) do { \
    close(sock_target_push); \
    close(sock_source_read); \
    free(curr_task); \
} while (0)

#define CLOSE_SOCKETS(sock_target_push, sock_source_read) do { \
    close(sock_target_push); \
    close(sock_source_read); \
} while (0)



typedef struct {
    sync_task_ts *queue;
    int log_fd;
    int write_console_sock;
} thread_args;

void *exec_task_manager_th(void *arg);



#endif