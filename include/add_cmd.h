#ifndef ADD_CMD_H
#define ADD_CMD_H


#include "nfs_manager_def.h"
#include "sync_task_queue.h"
#include "common_defs.h"

int enqueue_add_cmd(const manager_command curr_cmd, sync_task_ts *queue_tasks, sync_info_mem_store **sync_info_head,
    const char* source_full_path, const char *target_full_path, int fd_log, int write_sock);

int send_last_push_chunk(int sock_target_push, const char* filename, const char* target_dir);
int send_header(const int sock_target_push, const bool is_first_write, int request_bytes, const char *target_dir, const char *filename);
int send_push_first_header(const int sock_target_push, int request_bytes, const char *target_dir, const char *filename);
int send_push_header_generic(int sock_target_push, bool *is_first_push, int request_bytes, const char *target_dir, const char *filename);
int establish_connections_for_add_cmd(int *sock_source_read, int *sock_target_push, const sync_task *task);

int enqueue_config_pairs(int total_config_pairs, sync_info_mem_store **sync_info_head, sync_task_ts *queue_task, config_pairs *conf_pairs, 
    int fd_log, int write_sock);


#endif