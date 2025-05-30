#ifndef ADD_CMD_H
#define ADD_CMD_H


#include "common_defs.h"
#include "nfs_manager_def.h"
#include "sync_task.h"
#include "utils.h"


int enqueue_add_cmd(const manager_command curr_cmd, sync_task_ts *queue_tasks, sync_info_mem_store **sync_info_head,
    const char* source_full_path, const char *target_full_path);

int send_last_push_chunk(int sock_target_push, const char* filename, const char* target_dir);
int send_header(const int sock_target_push, const bool is_first_write, int request_bytes, const char *target_dir, const char *filename);
int send_push_first_header(const int sock_target_push, int request_bytes, const char *target_dir, const char *filename);
int send_push_header_generic(int sock_target_push, bool *is_first_push, int request_bytes, const char *target_dir, const char *filename);
int establish_connections_for_add_cmd(int *sock_source_read, int *sock_target_push, const sync_task *task);

#endif