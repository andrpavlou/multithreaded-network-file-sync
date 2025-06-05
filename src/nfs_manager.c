#include <fcntl.h>
#include <signal.h> 
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "utils.h"
#include "sync_task.h"
#include "sync_info.h"
#include "nfs_manager_def.h"
#include "common_defs.h"
#include "add_cmd.h"
#include "cancel_cmd.h"
#include "logging_defs.h"

#define DEBUG

volatile sig_atomic_t manager_active = 1;
void print_queue(sync_task_ts *task);


void handle_sigint(int sig){
    manager_active = 0;
}



typedef struct {
    sync_task_ts *queue;
    int log_fd;
    int write_console_sock;
} thread_args;


void *thread_exec_task(void *arg){
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
                    LOG_PUSH_SUCCESS(curr_task, (ssize_t) 0, fd_log);
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
            
            LOG_PULL_SUCCESS(curr_task, total_read_pull, fd_log);
            LOG_PUSH_SUCCESS(curr_task, total_write_push, fd_log);
            CLEANUP(sock_target_push, sock_source_read, curr_task);
        }
        
    }
    return NULL;
}



// bin/nfs_manager -l logs/manager.log -c config.txt -n 5 -p 2525 -b 10
int main(int argc, char *argv[]){
    char *logfile       = NULL;
    char *config_file   = NULL;
    
    int port            = -1;
    int worker_limit    = -1;
    int buffer_size     = -1;

    int check_args_status = check_args_manager(argc, argv, &logfile, &config_file, &worker_limit, &port, &buffer_size);
    if(check_args_status == 1){
        perror("USAGE: ./nfs_manager -l <logfile> -c <config_file> -n <worker_limit> -p <port> -b <buffer_size>");
        return 1;
    }
    if(check_args_status == -1){
        perror("ERROR: -b Must be > 0");
        return 1;
    }

    int fd_log;
    if((fd_log = open(logfile, O_WRONLY | O_TRUNC | O_CREAT), 0664) < 0){
        #ifdef DEBUG
        perror("open logfile");
        #endif
        return 1;
    }

    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; 
    sigaction(SIGINT, &sa, NULL);


    // Config file parsing
    int fd_config;
    if((fd_config = open(config_file, O_RDONLY)) < 0){
        #ifdef DEBUG
        perror("Config file open problem");
        #endif
        return 1;
    }

    int total_config_pairs = line_counter_of_file(fd_config, BUFFSIZ);
    lseek(fd_config, 0, SEEK_SET); // Reset the pointer at the start of the file
    config_pairs *conf_pairs = malloc(sizeof(config_pairs) * total_config_pairs);
    create_cf_pairs(fd_config, conf_pairs);
    
    
    sync_task_ts queue_tasks;
    if(init_sync_task_ts(&queue_tasks, buffer_size)){
        #ifdef DEBUG
        perror("init sync task");
        #endif
        
        free(conf_pairs);
        return 1;
    }


    sync_info_mem_store *sync_info_head = NULL;


    struct sockaddr_in server, client;
    struct sockaddr *serverptr = (struct sockaddr *) &server;
    struct sockaddr *clientptr = (struct sockaddr *) &client;

    int socket_manager = 0;
    if((socket_manager = socket(AF_INET , SOCK_STREAM , 0)) < 0){
        #ifdef DEBUG
        perror("socket");
        #endif
    }
    
    int optval = 1;
    setsockopt(socket_manager, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    server.sin_family       = AF_INET; /* Internet domain */
    server.sin_addr.s_addr  = htonl(INADDR_ANY);
    server.sin_port         = htons(port); /* The given port */
    
    /* Bind socket to address */
    if(bind(socket_manager, serverptr, sizeof(server)) < 0){
        #ifdef DEBUG
        perror("bind");
        #endif
        
        return 1;
    }

    if(listen(socket_manager, 5) < 0){
        #ifdef DEBUG
        perror("listen");
        #endif
        
        return 1;
    } 
    printf("Listening for connections to port % d \n", port);
    
    socklen_t clientlen     = sizeof(struct sockaddr_in);
    int socket_console_read = 0;
    if((socket_console_read = accept(socket_manager, clientptr, &clientlen)) < 0){
        #ifdef DEBUG
        perror("accept ");
        #endif

        return 1;
    }


    bool conf_pairs_sync = FALSE;

    // Thread pool, ready to execute tasks
    pthread_t *worker_th = malloc(worker_limit * sizeof(pthread_t));
    for(int i = 0; i < worker_limit; i++){
        thread_args *arglist = malloc(sizeof(thread_args));

        arglist->queue              = &queue_tasks;
        arglist->log_fd             = fd_log;
        arglist->write_console_sock = socket_console_read;

        pthread_create(&worker_th[i], NULL, thread_exec_task, arglist);
    }


    if(conf_pairs_sync == FALSE){
        enqueue_config_pairs(total_config_pairs, &sync_info_head, &queue_tasks, conf_pairs, fd_log, -1);
        conf_pairs_sync = TRUE;
    }
    
    
    printf("Accepted connection \n") ;
    while(manager_active){

        char read_buffer[BUFFSIZ];
        memset(read_buffer, 0, sizeof(read_buffer));
        
        ssize_t n_read;
        if((n_read = read(socket_console_read, read_buffer, BUFFSIZ - 1)) <= 0){
            #ifdef DEBUG
            perror("Read from console");
            #endif
            return 1;
        }
        read_buffer[n_read] = '\0';

        manager_command curr_cmd;
        char *source_full_path = NULL;
        char *target_full_path = NULL;
        if(parse_console_command(read_buffer, &curr_cmd, &source_full_path, &target_full_path)){
            #ifdef DEBUG
            fprintf(stderr, "error: parse_command [%s]\n", read_buffer);
            #endif

            write(socket_console_read, "END\n", 4);
            continue;
        }

        if(curr_cmd.op == ADD){
            int status = enqueue_add_cmd(curr_cmd, &queue_tasks, &sync_info_head, source_full_path, target_full_path, fd_log, socket_console_read);
            if(status){
                #ifdef DEBUG
                perror("enqueue add");
                #endif
            }

            free(source_full_path);
            free(target_full_path);

            write(socket_console_read, "END\n", 4);
        }

        if(curr_cmd.op == CANCEL){
            int status = enqueue_cancel_cmd(curr_cmd, &queue_tasks, &sync_info_head, source_full_path);
            if(status){
                LOG_CANCEL_NOT_MONITORED(curr_cmd, socket_console_read);
                write(socket_console_read, "END\n", 4);
            }

            // Thread will send "END\n"
            free(source_full_path);
        }

        if(curr_cmd.op == SHUTDOWN) manager_active = 0;
    }

    LOG_MANAGER_SHUTDOWN_SEQUENCE(socket_console_read); 


    // *********** QUEUE THE POISON PILLS *********** //
    for(int i = 0; i < worker_limit; i++){
        sync_task *shutdown_task = calloc(1, sizeof(sync_task));
        shutdown_task->manager_cmd.op = SHUTDOWN;

        enqueue_task(&queue_tasks, shutdown_task);
    }

    for(int i = 0; i < worker_limit; i++){
        pthread_join(worker_th[i], NULL);
    }

    LOG_MANAGER_SHUTDOWN_FINAL(socket_console_read);

    close(fd_log);
    free(conf_pairs);
    free(worker_th);
    close(socket_manager);
    close(socket_console_read);
    free_queue_task(&queue_tasks);
    free_all_sync_info(&sync_info_head);

    
    return 0;
}



static const char* cmd_to_str(manager_command cmd){
    if(cmd.op == ADD)       return "ADD";
    if(cmd.op == CANCEL)    return "CANCEL";
    if(cmd.op == SHUTDOWN)  return "SHUTDOWN";

    return "INVALID";
}

void print_queue(sync_task_ts *task){
    if(!task){
        printf("NULL task\n");
        return;
    }
    printf("Atomic Counter: %d\n", task->cancel_cmd_counter);
    for(int i = 0; i < task->size; i++){
        int index = (task->head + i) % task->buffer_slots;
        sync_task *curr_task = task->tasks_array[index];

        printf("Operation\t: %s\n", cmd_to_str(curr_task->manager_cmd));
        if(curr_task->manager_cmd.op == ADD){
            printf("Filename\t: %s\n", curr_task->filename);
            printf("Source Dir\t: %s\n", curr_task->manager_cmd.source_dir);
            printf("Source IP\t: %s\n", curr_task->manager_cmd.source_ip);
            printf("Source Port\t: %d\n", curr_task->manager_cmd.source_port);
            printf("Target Dir\t: %s\n", curr_task->manager_cmd.target_dir);
            printf("Target IP\t: %s\n", curr_task->manager_cmd.target_ip);
            printf("Target Port\t: %d\n", curr_task->manager_cmd.target_port);
        }


        if(curr_task->manager_cmd.op == CANCEL){
            printf("Source Dir\t: %s\n", curr_task->manager_cmd.cancel_dir);
            printf("Source IP\t: %s\n", curr_task->manager_cmd.source_ip);
            printf("Source Port\t: %d\n", curr_task->manager_cmd.source_port);
        }


        if(curr_task->node){
            printf("Sync Info:\n");
            printf("  Source\t: %s\n", curr_task->node->source);
            printf("  Target\t: %s\n", curr_task->node->target);
        } else {
            printf("Sync Info: NULL\n");
        }
        printf("----------------------------------\n");
    }
}
