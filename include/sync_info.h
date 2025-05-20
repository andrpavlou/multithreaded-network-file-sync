#ifndef SYNC_INFO_H
#define SYNC_INFO_H

#include "nfs_manager.h"


#define BUFFSIZ 256
#define EVENT_BUFSIZ (1024 * (sizeof(struct inotify_event) + NAME_MAX + 1))


typedef struct sync_info_mem_store {
    char source[BUFFSIZ];
    char target[BUFFSIZ];
    time_t last_sync;
    int error_count;
    int active;         
    int watch_fd;
    pid_t worker_pid;
    int read_fd_from_worker;
    struct sync_info_mem_store* next;
} sync_info_mem_store;

sync_info_mem_store* find_by_watch_fd(sync_info_mem_store* head, int wd);

int init_sync_info(sync_info_mem_store** head);
int add_sync_info(sync_info_mem_store** head, const char* source, const char* target);
sync_info_mem_store* find_sync_info(sync_info_mem_store* head, const char* source);
int remove_sync_info(sync_info_mem_store** head, const char* source);
void print_all_sync_info(sync_info_mem_store* head);
void free_all_sync_info(sync_info_mem_store** head);
sync_info_mem_store* find_by_pid(sync_info_mem_store *head, pid_t pid);

#endif
