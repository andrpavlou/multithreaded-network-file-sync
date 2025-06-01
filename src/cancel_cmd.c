#include "cancel_cmd.h"
#include "sync_info.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


int enqueue_cancel_cmd(const manager_command curr_cmd, sync_task_ts *queue_tasks, sync_info_mem_store **sync_info_head, 
    const char* source_full_path){

    int host_count      = 0;
    char **found_hosts  = find_sync_info_hosts_by_dir((*sync_info_head), curr_cmd.cancel_dir, &host_count);
    if(!host_count) return 1;
    

    printf("\n--------------------\n");
    for(int i = 0; i < host_count; i++){

        int current_parsed_port = -1;
        char current_parsed_ip[BUFFSIZ];

        int status = parse_path(found_hosts[i], NULL, current_parsed_ip, &current_parsed_port);
        if(status){
            printf("idx: %d found host: %s counter: %d\n", i, found_hosts[i], host_count);
            fflush(stdout);
            perror("parse path for cancel");
            continue;
        }
        if(task_exists_cancel(queue_tasks, (const char *)current_parsed_ip, (const char *)curr_cmd.cancel_dir, current_parsed_port) == TRUE){
            printf("CANCEL ALREADY QUEUED.......\n"); // proper logging
            fflush(stdout);
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
    printf("\n--------------------\n");
    fflush(stdout);

    return 0;
}