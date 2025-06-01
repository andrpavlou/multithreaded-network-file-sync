#include "add_cmd.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int enqueue_add_cmd(const manager_command curr_cmd, sync_task_ts *queue_tasks, sync_info_mem_store **sync_info_head,
    const char* source_full_path, const char *target_full_path){
    

    int sock_source_read;
    if(establish_connection(&sock_source_read, curr_cmd.source_ip, curr_cmd.source_port)){
        perror("establish con");
        return 1;
    }   

    char list_cmd_buff[BUFFSIZ];
    int list_len = snprintf(list_cmd_buff, sizeof(list_cmd_buff), "LIST %s\r\n", curr_cmd.source_dir);


    // //========== Request List Command ========== //
    if(write_all(sock_source_read, list_cmd_buff, list_len) == -1){
        close(sock_source_read);
        perror("list write");
        return 1;
    }

    char *list_reply_buff = NULL;
    read_list_response(sock_source_read, &list_reply_buff);


    char *file_buff[MAX_FILES];
    unsigned int total_src_files = get_files_from_list_response(list_reply_buff, file_buff);
    

    for(int i = 0; i < total_src_files; i++){
        if(task_exists_add(queue_tasks, file_buff[i], curr_cmd) == TRUE){
            printf("DIR ALREADY MONITORED.......\n"); // proper logging
            fflush(stdout);
            continue;
        }


        sync_task *new_task         = malloc(sizeof(sync_task));
        new_task->manager_cmd.op    = curr_cmd.op;

        ssize_t manager_buff_size   = sizeof(new_task->manager_cmd.source_ip);

        // copy source ip/port
        new_task->manager_cmd.source_port = curr_cmd.source_port;
        strncpy(new_task->manager_cmd.source_ip, curr_cmd.source_ip, manager_buff_size - 1);
        new_task->manager_cmd.source_ip[manager_buff_size - 1] = '\0';

        // copy target ip/port
        new_task->manager_cmd.target_port = curr_cmd.target_port;
        strncpy(new_task->manager_cmd.target_ip, curr_cmd.target_ip, manager_buff_size - 1);
        new_task->manager_cmd.target_ip[manager_buff_size - 1] = '\0';

        // copy source dir
        strncpy(new_task->manager_cmd.source_dir, curr_cmd.source_dir, manager_buff_size - 1);
        new_task->manager_cmd.source_dir[manager_buff_size - 1] = '\0';

        // copy target dir
        strncpy(new_task->manager_cmd.target_dir, curr_cmd.target_dir, manager_buff_size - 1);
        new_task->manager_cmd.target_dir[manager_buff_size - 1] = '\0';

        // current file
        strncpy(new_task->filename, file_buff[i], sizeof(new_task->filename) - 1);
        new_task->filename[sizeof(new_task->filename) - 1] = '\0';

        // Check if the source full path is already inserted and keep that node, otherwise just insert it
        sync_info_mem_store *find_node;
        if((find_node = find_sync_info((*sync_info_head), source_full_path, target_full_path)) != NULL){
            new_task->node = find_node;
            enqueue_task(queue_tasks, new_task);

            continue;
        } 
        
        sync_info_mem_store *inserted_node = add_sync_info(sync_info_head, source_full_path, target_full_path);
        if(inserted_node == NULL) {
            perror("sync info insert");    
            
            free(new_task);
            close(sock_source_read);
            return 1;
        } 

        new_task->node = inserted_node;
        enqueue_task(queue_tasks, new_task);
    }

    free(list_reply_buff);
    close(sock_source_read);
    return 0;
}




int send_last_push_chunk(int sock_target_push, const char* filename, const char* target_dir){
    // Request_bytes = 0 -> just send push request with chunk size = 0 indicating the last push
    char push_buffer[BUFFSIZ];
    int push_len = snprintf(push_buffer, sizeof(push_buffer), "PUSH %s/%s %d\r\n", 
                                        target_dir,
                                        filename,
                                        0);
    if(write_all(sock_target_push, push_buffer, push_len) == -1){
        return 1;
    }

    return 0;
}


/*
* Send -1 batch size to create the file if it's the first write,
* otherwise send the bytes read from other host -> request_bytes
*/
int send_header(const int sock_target_push, const bool is_first_write, int request_bytes, const char *target_dir, const char *filename){
    char header_buffer[BUFFSIZ];
    int write_batch_bytes   = is_first_write == TRUE ? -1 : request_bytes; 
    int header_len          = snprintf(header_buffer, sizeof(header_buffer), "PUSH %s/%s %d\r\n", 
                                                                                    target_dir, 
                                                                                    filename, 
                                                                                    write_batch_bytes);

    if(header_len < 0 || header_len >= (int)sizeof(header_buffer)){
        perror("header too long");
        return 1;
    }
    
    if(write_all(sock_target_push, header_buffer, header_len) == -1){
        fprintf(stderr, "Send header push\nFirst write:\t%s\nTarget Dir:\t%s\nFilename:\t%s\n", 
            is_first_write == TRUE ? "TRUE" : "FALSE",
            target_dir, 
            filename);
        return 1;
    }

    return 0;
}




int send_push_first_header(const int sock_target_push, int request_bytes, const char *target_dir, const char *filename){
    if(send_header(sock_target_push, TRUE, request_bytes, target_dir, filename)){
        return 1;
    }

    if(send_header(sock_target_push, FALSE, request_bytes, target_dir, filename)){
        return 1;
    }

    return 0;
}


int send_push_header_generic(int sock_target_push, bool *is_first_push, int request_bytes, const char *target_dir, const char *filename){
    if(*is_first_push == TRUE){
        if(send_push_first_header(sock_target_push, request_bytes, target_dir, filename)){
            return 1;
        }

        *is_first_push = FALSE;
        return 0;
    }

    if(send_header(sock_target_push, *is_first_push, request_bytes, target_dir, filename)){
        return 1;
    }

    return 0;
}




// No need to close() after this fails
int establish_connections_for_add_cmd(int *sock_source_read, int *sock_target_push, const sync_task *task){
    if(establish_connection(sock_source_read, task->manager_cmd.source_ip, task->manager_cmd.source_port)){
        perror("establish con source");
        return 1;
    }
    
    if(establish_connection(sock_target_push, task->manager_cmd.target_ip, task->manager_cmd.target_port)){
        perror("establish con target");
        return 1;
    }  

    return 0;
}