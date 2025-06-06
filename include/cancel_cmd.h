#ifndef CANCEL_CMD_H
#define CANCEL_CMD_H


#include "nfs_manager_def.h"
#include "sync_task_queue.h"

int enqueue_cancel_cmd(const manager_command curr_cmd, sync_task_ts *queue_tasks, sync_info_mem_store **sync_info_head, 
    const char* source_full_path);


int remove_canceled_add_tasks(sync_task_ts *task, sync_task *cancel_task, int fd_log);


#endif