#ifndef SYNC_INFO_H
#define SYNC_INFO_H



#include "common_defs.h"
#include <stdatomic.h>
#include <pthread.h>



typedef struct sync_info_mem_store {
    char source[BUFFSIZ];
    char target[BUFFSIZ];

    atomic_long last_sync;
    atomic_int error_count;
    atomic_int active;  
           
    int report_fd;
    pid_t worker_pid;
    struct sync_info_mem_store* next;
} sync_info_mem_store;


sync_info_mem_store* add_sync_info(sync_info_mem_store** head, const char* source, const char* target);
sync_info_mem_store* find_sync_info(sync_info_mem_store* head, const char* source, const char* target);
int remove_sync_info(sync_info_mem_store** head, const char* source);
void free_all_sync_info(sync_info_mem_store** head);
sync_info_mem_store* find_by_pid(sync_info_mem_store *head, pid_t pid);
char** find_sync_info_hosts_by_dir(sync_info_mem_store *head, const char* dir, int *cancel_dir_count);

void print_all_sync_info(sync_info_mem_store* head);
#endif
