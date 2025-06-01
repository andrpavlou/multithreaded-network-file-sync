#ifndef CANCEL_CMD_H
#define CANCEL_CMD_H


#include "nfs_manager_def.h"
#include "sync_task.h"

int enqueue_cancel_cmd(const manager_command curr_cmd, sync_task_ts *queue_tasks, sync_info_mem_store **sync_info_head, 
    const char* source_full_path);

#endif