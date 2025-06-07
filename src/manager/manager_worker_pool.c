#include "manager_worker_pool.h"
#include "logging_defs.h"
#include "socket_utils.h"
#include "common_defs.h"
#include "cancel_cmd.h"
#include "add_cmd.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>


void *exec_task_manager_th(void *arg){
    thread_args *args = (thread_args*) arg;
    sync_task_ts *queue_tasks = args->queue;
    
    int fd_log          = args->log_fd;
    int write_socket    = args->write_console_sock;

    free(args);
    
    while(manager_active || queue_tasks->size){
        sync_task *curr_task = dequeue_task(queue_tasks);
        if(curr_task == NULL){
            #ifdef DEBUG
            perror("curr task is null");
            break;
            #endif
        }

        if(curr_task->manager_cmd.op == SHUTDOWN){
            free(curr_task);
            break;
        }

        if(curr_task->manager_cmd.op == CANCEL){
            int removed_count = remove_canceled_add_tasks(queue_tasks, curr_task, fd_log);
            
            if(removed_count){
                LOG_CANCEL_SUCCESS(curr_task, fd_log, write_socket);
            } else {
                LOG_CANCEL_NOT_MONITORED_THREAD(curr_task, write_socket);
            }
            
            /* 
             * When inputted "cancel" is possible to have 1+ cancel queued task, simultaneously,
             * for the same dir if it exists in multiple hosts so,
             * (1 command -> multiple tasks queued). It is  Important to keep track of 
             * the lastthread exiting to send "END\n" to console to avoid race condition. 
             */
            if(atomic_fetch_sub(&queue_tasks->cancel_cmd_counter, 1) == 1){
                write(write_socket, "END\n", 4);
            }

            free(curr_task);
            continue;
        }
    
        if(curr_task->manager_cmd.op == ADD){
            int sock_source_read;
            int sock_target_push;

            // source/target connection
            if(establish_connections_for_add_cmd(&sock_source_read, &sock_target_push, curr_task)){
                free(curr_task);
                continue;
            }

            char pull_request_cmd_buff[BUFFSIZ];
            memset(pull_request_cmd_buff, 0, sizeof(pull_request_cmd_buff));

            int len_pull_req = snprintf(pull_request_cmd_buff, sizeof(pull_request_cmd_buff), 
                                    "PULL %s/%s\r\n",
                                    curr_task->manager_cmd.source_dir,
                                    curr_task->filename);
            
            // Request pull of given file
            if(write_all(sock_source_read, pull_request_cmd_buff, len_pull_req) == -1){
                #ifdef DEBUG
                perror("pull write");
                #endif

                CLOSE_SOCKETS(sock_target_push, sock_source_read);
                break;
            }
            
            long file_size = 0;
            if((file_size = get_file_size_of_host(sock_source_read)) == -1){
                char err_buff[BUFFSIZ];

                int len = read_next_line_from_fd(sock_source_read, err_buff, sizeof(err_buff));
                err_buff[len - 1] = '\0';
            
                LOG_PULL_ERROR(curr_task, err_buff, fd_log);

                CLEANUP(sock_target_push, sock_source_read, curr_task);
                continue;
            }

            if(file_size == 0){
                bool is_first_write = TRUE;
                int status = send_push_header_generic(sock_target_push, &is_first_write, file_size, curr_task->manager_cmd.target_dir, curr_task->filename);
                if(status){
                    #ifdef DEBUG
                    perror("send push header");
                    #endif
                } else {
                    LOG_PUSH_SUCCESS(curr_task, (ssize_t) 0, fd_log, file_size);
                }

                CLEANUP(sock_target_push, sock_source_read, curr_task);
                continue;
            }
            
            
            char pull_buffer[BUFFSIZ];
            bool is_first_push              = TRUE;
            ssize_t total_write_push        = 0;
            ssize_t total_read_pull         = 0;
            while(total_write_push <= file_size){
                memset(pull_buffer, 0, sizeof(pull_buffer)); 
                
                ssize_t request_bytes = MIN((file_size - total_write_push), BUFFSIZ - 1);
                if(!request_bytes){
                    if(send_last_push_chunk(sock_target_push, curr_task->filename, curr_task->manager_cmd.target_dir)){
                        #ifdef DEBUG
                        perror("send_last_push_chunk");
                        #endif
                    }
                    break;
                }

                ssize_t n_read;
                if((n_read = read_all(sock_source_read, pull_buffer, request_bytes)) == -1){
                    #ifdef DEBUG
                    perror("\nread pull_buffer");
                    #endif
                    break;
                }
                total_read_pull += n_read;

                if(send_push_header_generic(sock_target_push, &is_first_push, request_bytes, curr_task->manager_cmd.target_dir, curr_task->filename)){
                    break;
                }

                ssize_t write_b;
                if((write_b = write_all(sock_target_push, pull_buffer, request_bytes)) == -1){
                    #ifdef DEBUG
                    perror("write push");
                    #endif

                    break;
                }
                
                total_write_push += write_b;
            }
            
            LOG_PULL_SUCCESS(curr_task, total_read_pull, fd_log, file_size);
            LOG_PUSH_SUCCESS(curr_task, total_write_push, fd_log, file_size);
            CLEANUP(sock_target_push, sock_source_read, curr_task);
        }
        
    }
    return NULL;
}

