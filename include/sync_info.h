#ifndef SYNC_INFO_H
#define SYNC_INFO_H

#include <stdio.h>
#include <sys/types.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define BUFFSIZ 256
#define BUFFSIZ_CHARS 3



typedef struct sync_info_mem_store {
    char source[BUFFSIZ];
    char target[BUFFSIZ];
    time_t last_sync;
    int error_count;
    int active;         
    // int watch_fd;
    int report_fd;
    pid_t worker_pid;

    struct sync_info_mem_store* next;
} sync_info_mem_store;


int add_sync_info(sync_info_mem_store** head, const char* source, const char* target);
sync_info_mem_store* find_sync_info(sync_info_mem_store* head, const char* source);
int remove_sync_info(sync_info_mem_store** head, const char* source);
void free_all_sync_info(sync_info_mem_store** head);
sync_info_mem_store* find_by_pid(sync_info_mem_store *head, pid_t pid);



// void print_all_sync_info(sync_info_mem_store* head);
// sync_info_mem_store* find_by_watch_fd(sync_info_mem_store* head, int wd);
#endif
