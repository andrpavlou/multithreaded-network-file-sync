#include "cancel_cmd.h"
#include "sync_info.h"
#include "utils.h"
#include "logging_defs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


int enqueue_cancel_cmd(const manager_command curr_cmd, sync_task_ts *queue_tasks, sync_info_mem_store **sync_info_head, 
    const char* source_full_path){

    int host_count      = 0;
    char **found_hosts  = find_sync_info_hosts_by_dir((*sync_info_head), curr_cmd.cancel_dir, &host_count);
    if(!host_count) return 1;
    

    for(int i = 0; i < host_count; i++){

        int current_parsed_port = -1;
        char current_parsed_ip[BUFFSIZ];

        int status = parse_path(found_hosts[i], NULL, current_parsed_ip, &current_parsed_port);
        if(status){
            #ifdef DEBUG
            perror("parse path for cancel");
            #endif
            continue;
        }
        if(task_exists_cancel(queue_tasks, (const char *)current_parsed_ip, (const char *)curr_cmd.cancel_dir, current_parsed_port) == TRUE){
            continue;
        }

        
        sync_task *new_task         = malloc(sizeof(sync_task));
        new_task->manager_cmd.op    = CANCEL;

        ssize_t manager_buff_size   = sizeof(new_task->manager_cmd.cancel_dir);

        // Copy cancelling dir
        strncpy(new_task->manager_cmd.cancel_dir, curr_cmd.cancel_dir, manager_buff_size - 1);
        new_task->manager_cmd.cancel_dir[manager_buff_size - 1] = '\0';

        // Copy source ip of the cancelling dir
        strncpy(new_task->manager_cmd.source_ip, current_parsed_ip, manager_buff_size - 1);
        new_task->manager_cmd.source_ip[manager_buff_size - 1] = '\0';
        
        // Copy source port of the cancelling dir
        new_task->manager_cmd.source_port = current_parsed_port;

        // No need for error checking, because it can not be null due earlier check with: find_sync_info_hosts_by_dir
        sync_info_mem_store *curr_cancel_dir_node = find_sync_info((*sync_info_head), found_hosts[i], NULL);
        new_task->node                      = curr_cancel_dir_node;

        new_task->filename[0]               = '\0';
        new_task->manager_cmd.target_ip[0]  = '\0';
        new_task->manager_cmd.target_port   = -1;
        
        enqueue_task(queue_tasks, new_task);
        
        free(found_hosts[i]);
        fflush(stdout);
    }

    free(found_hosts);
    fflush(stdout);

    return 0;
}





/*
*  Removes all the queued tasks with "ADD" operation, in relation to the cancel task as:
*       1) A task with "ADD" operation
*       2) Same port
*       3) Same IPs
*       4) Source dir = Canceled dir
*
*   If a task matches these, it should be removed. In case of removing a task; the next task that will not be 
*   removed should be shifted to the index that is now empty in the middle.acct
*/

int remove_canceled_add_tasks(sync_task_ts *queue, sync_task *cancel_task, int fd_log){
    pthread_mutex_lock(&queue->mutex);

    int removed_count   = 0;
    int next_write_idx  = queue->head; // Holds index position to write in the next, non removed task (if curr_idx != next_write_idx).

    for(int i = 0; i < queue->size; i++){
        int curr_idx = (queue->head + i) % queue->buffer_slots;
        sync_task *curr_task = queue->tasks_array[curr_idx];

        int should_remove =     curr_task->manager_cmd.op == ADD                                                            &&
                                curr_task->manager_cmd.source_port ==  cancel_task->manager_cmd.source_port                 &&
                                !strncmp(curr_task->manager_cmd.source_dir, cancel_task->manager_cmd.cancel_dir, BUFFSIZ)   &&
                                !strncmp(curr_task->manager_cmd.source_ip, cancel_task->manager_cmd.source_ip, BUFFSIZ);
        
        if(should_remove){
            removed_count++;

            free(curr_task);
            continue;
        }
        
        // Means we have an empty position in the middle, so the current_task should be shifted back, filling the empty index.
        if(curr_idx != next_write_idx){
            queue->tasks_array[next_write_idx] = curr_task;
        }

        // Update this no mater if (curr_idx != next_write_idx) is true or not
        next_write_idx = (next_write_idx + 1) % queue->buffer_slots;
    }

    queue->tail = next_write_idx;
    queue->size -= removed_count;

    pthread_mutex_unlock(&queue->mutex);

    return removed_count;
}