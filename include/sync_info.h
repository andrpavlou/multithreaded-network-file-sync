#ifndef SYNC_INFO_H
#define SYNC_INFO_H

#include <stdio.h>
#include <sys/types.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>

#define BUFFSIZ 256
#define BUFFSIZ_CHARS 3



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
sync_info_mem_store* find_sync_info(sync_info_mem_store* head, const char* source);
int remove_sync_info(sync_info_mem_store** head, const char* source);
void free_all_sync_info(sync_info_mem_store** head);
sync_info_mem_store* find_by_pid(sync_info_mem_store *head, pid_t pid);
void find_sync_info_by_dir(sync_info_mem_store *head, const char* dir);


// void print_all_sync_info(sync_info_mem_store* head);
#endif
