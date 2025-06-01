#include "utils.h"
#include "sync_task.h"
#include "sync_info.h"
#include "nfs_manager_def.h"
#include "common.h"
#include "common_defs.h"
#include "add_cmd.h"
#include "cancel_cmd.h"



/*
  socket()
    |
  connect() 
    |
  write()
    |
  read()
*/

void print_queue(sync_task_ts *task);
void shift_queue(sync_task_ts *task);

volatile sig_atomic_t manager_active = 1;


void handle_sigint(int sig){
    manager_active = 0;
}


int parse_console_command(const char* buffer, manager_command *full_command, char **source_full_path, char **target_full_path){

    if(!strncasecmp(buffer, "CANCEL", 6)){
        full_command->op = CANCEL;

        buffer += 6; // Move past cancel "cancel"
        while(isspace(*buffer)) buffer++; // Skip spaces between CANCEL and PATH in case more exist
        
        char temp[BUFSIZ];
        strncpy(temp, buffer, BUFSIZ - 1);
        temp[strcspn(temp, "\n")] = '\0';   // remove newline


        (*source_full_path) = strtok(temp, " ");
        // If the current first_token is null or more text exists after first_token with spaces
        // we should return error
        if((*source_full_path) == NULL || strtok(NULL, " ") != NULL) return 1;


        strncpy(full_command->cancel_dir, (*source_full_path), BUFFSIZ - 1);

        full_command->source_ip[BUFFSIZ - 1] = '\0';
        full_command->target_ip[0] = '\0';

        (*target_full_path) = NULL;
        return 0;
    }


    if(!strncasecmp(buffer, "ADD", 3)){
        full_command->op = ADD;

        buffer += 3;                        // Move past cancel "add"
        while(isspace(*buffer)) buffer++;   // Skip spaces between add and source in case more exist
        
        char temp[BUFSIZ];
        strncpy(temp, buffer, BUFSIZ - 1);
        temp[strcspn(temp, "\n")] = '\0';   // remove newline


        (*source_full_path) = strtok(temp, " ");  // Source path
        if((*source_full_path) == NULL) return 1;

        int status = parse_path((*source_full_path), full_command->source_dir, full_command->source_ip, &full_command->source_port);
        if(status == -1){
            perror("parse path target");
        }
        full_command->source_ip[BUFFSIZ - 1] = '\0';

        
        (*target_full_path) = strtok(NULL, " "); // Target path
        if((*target_full_path) == NULL) return 1;

        
        parse_path((*target_full_path), full_command->target_dir, full_command->target_ip, &full_command->target_port);
        full_command->target_ip[BUFFSIZ - 1] = '\0';

        char *end = strtok(NULL, " ");  // If the user has given extra arguments, the command is not accepted
        if(end != NULL) return 1;
        
        return 0;
    }

    if(!strncasecmp(buffer, "SHUTDOWN", 8)){
        full_command->op = SHUTDOWN;

        full_command->source_ip[0] = '\0';
        full_command->target_ip[0] = '\0';

        return 0;
    }

    return 1;
}





void *thread_exec_task(void *arg){
    sync_task_ts *queue_tasks   = (sync_task_ts*)arg;
    
    while(manager_active || queue_tasks->size){
        // if(queue_tasks->cancel_flag == 1){
        //     printf("\nFLAG IS ENABLED\n");
        // }
        sync_task *curr_task = dequeue_task(queue_tasks);
        if(curr_task == NULL){
            perror("curr task is null");
            break;
        }

        if(curr_task->manager_cmd.op == SHUTDOWN){
            printf("Thread %lu shutting down\n", pthread_self());
            
            free(curr_task);
            break;
        }

        if(curr_task->manager_cmd.op == CANCEL){
            printf("\n\n\nTHREAD CANCELLING:\n");
        
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
                perror("pull write");

                CLOSE_SOCKETS(sock_target_push, sock_source_read);
                break;
            }
            
            long file_size = 0;
            if((file_size = get_file_size_of_host(sock_source_read)) == -1){
                perror("get_file_size_of_host");
             
                CLEANUP(sock_target_push, sock_source_read, curr_task);
                continue;
            }

            if(file_size == 0){
                bool is_first_write = TRUE;
                send_push_header_generic(sock_target_push, &is_first_write, file_size, curr_task->manager_cmd.target_dir, curr_task->filename);
                CLEANUP(sock_target_push, sock_source_read, curr_task);
                continue;
            }
            
            
            char pull_buffer[BUFFSIZ];
            bool is_first_push              = TRUE;
            ssize_t total_read_pull_req     = 0;

            while(total_read_pull_req <= file_size){
                memset(pull_buffer, 0, sizeof(pull_buffer)); 
                
                ssize_t request_bytes = MIN((file_size - total_read_pull_req), BUFFSIZ - 1);
                if(!request_bytes){
                    if(send_last_push_chunk(sock_target_push, curr_task->filename, curr_task->manager_cmd.target_dir)){
                        perror("send_last_push_chunk");
                    }

                    CLOSE_SOCKETS(sock_target_push, sock_source_read);
                    break;
                }

                ssize_t n_read;
                if((n_read = read_all(sock_source_read, pull_buffer, request_bytes)) <= 0){
                    perror("\nread pull_buffer");
                 
                    CLEANUP(sock_target_push, sock_source_read, curr_task);
                    continue;
                }
                

                if(send_push_header_generic(sock_target_push, &is_first_push, request_bytes, curr_task->manager_cmd.target_dir, curr_task->filename)){
                    CLEANUP(sock_target_push, sock_source_read, curr_task);
                    continue;
                }

                
                if(write_all(sock_target_push, pull_buffer, request_bytes) == -1){
                    perror("write push");
                    CLEANUP(sock_target_push, sock_source_read, curr_task);
                    continue;
                }
                
                total_read_pull_req += n_read;
            }
            CLEANUP(sock_target_push, sock_source_read, curr_task);
        }
        
    }
    return NULL;
}


int enqueue_config_pairs(int total_config_pairs, sync_info_mem_store **sync_info_head, sync_task_ts *queue_task, config_pairs *conf_pairs){
    for(int i = 0; i < total_config_pairs; i++){
        manager_command conf_cmd;
        conf_cmd.op = ADD;

        strncpy(conf_cmd.source_dir, conf_pairs[i].source_dir_path, BUFFSIZ - 1);
        conf_cmd.source_dir[BUFFSIZ - 1] = '\0';

        strncpy(conf_cmd.source_ip, conf_pairs[i].source_ip, BUFFSIZ / 4 - 1);
        conf_cmd.source_ip[BUFFSIZ / 4 - 1] = '\0';

        conf_cmd.source_port = conf_pairs[i].source_port;

        strncpy(conf_cmd.target_dir, conf_pairs[i].target_dir_path, BUFFSIZ - 1);
        conf_cmd.target_dir[BUFFSIZ - 1] = '\0';

        strncpy(conf_cmd.target_ip, conf_pairs[i].target_ip, BUFFSIZ / 4 - 1);
        conf_cmd.target_ip[BUFFSIZ / 4 - 1] = '\0';

        conf_cmd.target_port = conf_pairs[i].target_port;


        int status = enqueue_add_cmd(conf_cmd, queue_task, sync_info_head, conf_pairs[i].source_full_path, conf_pairs[i].target_full_path);
        if(status){
            perror("enqueue enqueue ");
            return 1;
        }
    }
    return 0;
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


    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; 
    sigaction(SIGINT, &sa, NULL);


    // Config file parsing
    int fd_config;
    if((fd_config = open(config_file, O_RDONLY)) < 0){
        perror("Config file open problem");
        exit(3);
    }

    int total_config_pairs = line_counter_of_file(fd_config, BUFFSIZ);
    lseek(fd_config, 0, SEEK_SET); // Reset the pointer at the start of the file
    config_pairs *conf_pairs = malloc(sizeof(config_pairs) * total_config_pairs);
    create_cf_pairs(fd_config, conf_pairs);
    
    
    sync_task_ts queue_tasks;
    if(init_sync_task_ts(&queue_tasks, buffer_size)){
        free(conf_pairs);

        perror("init sync task");
        return 1;
    }


    sync_info_mem_store *sync_info_head = NULL;


    struct sockaddr_in server, client;
    struct sockaddr *serverptr = (struct sockaddr *) &server;
    struct sockaddr *clientptr = (struct sockaddr *) &client;

    int socket_manager = 0;
    if((socket_manager = socket(AF_INET , SOCK_STREAM , 0)) < 0)
        perror("socket");
    
    int optval = 1;
    setsockopt(socket_manager, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    server.sin_family       = AF_INET; /* Internet domain */
    server.sin_addr.s_addr  = htonl(INADDR_ANY);
    server.sin_port         = htons(port); /* The given port */
    
    /* Bind socket to address */
    if(bind(socket_manager, serverptr, sizeof(server)) < 0){
        perror("bind");
        exit(1);
    }

    if(listen(socket_manager, 5) < 0) perror ("listen");
    
    printf("Listening for connections to port % d \n", port);
    
    socklen_t clientlen     = sizeof(struct sockaddr_in);
    int socket_console_read = 0;
    if((socket_console_read = accept(socket_manager, clientptr, &clientlen)) < 0){
        perror("accept ");
        return 1;
    }

    atomic_int conf_pairs_sync = 0;

    // pthread_t *worker_th = malloc(worker_limit * sizeof(pthread_t));
    // for(int i = 0; i < worker_limit; i++){
    //     pthread_create(&worker_th[i], NULL, thread_exec_task, &queue_tasks);
    // }


    if(!conf_pairs_sync){
        enqueue_config_pairs(total_config_pairs, &sync_info_head, &queue_tasks, conf_pairs);
        conf_pairs_sync = 1;
    }
    


    printf("Accepted connection \n") ;
    while(manager_active){

        char read_buffer[BUFFSIZ];
        memset(read_buffer, 0, sizeof(read_buffer));
        
        ssize_t n_read;
        if((n_read = read(socket_console_read, read_buffer, BUFFSIZ - 1)) <= 0){
            perror("Read from console");
            return 1;
        }
        read_buffer[n_read] = '\0';

        manager_command curr_cmd;
        char *source_full_path, *target_full_path;
        if(parse_console_command(read_buffer, &curr_cmd, &source_full_path, &target_full_path)){
            fprintf(stderr, "error: parse_command [%s]\n", read_buffer);
            continue;
        }

        if(curr_cmd.op == ADD){
            int status = enqueue_add_cmd(curr_cmd, &queue_tasks, &sync_info_head, source_full_path, target_full_path);
            if(status){
                perror("enqueue add");
            }
        }

        if(curr_cmd.op == CANCEL){
            int status = enqueue_cancel_cmd(curr_cmd, &queue_tasks, &sync_info_head, source_full_path);
            if(status){
                printf("\n----- Dir not Monitored ---------\n");
                fflush(stdout);
                continue;
            }
        }

        if(curr_cmd.op == SHUTDOWN){
            manager_active = 0;
            continue;
        }

    }

    // for(int i = 0; i < worker_limit; i++){
    //     sync_task *shutdown_task = malloc(sizeof(sync_task));
    //     shutdown_task->manager_cmd.op = SHUTDOWN;
    
    //     shutdown_task->node                         = NULL;
    //     shutdown_task->filename[0]                  = '\0';
    //     shutdown_task->manager_cmd.full_path[0]     = '\0';
    //     shutdown_task->manager_cmd.source_ip[0]     = '\0';
    //     shutdown_task->manager_cmd.target_ip[0]     = '\0';
    //     shutdown_task->manager_cmd.source_dir[0]    = '\0';
    //     shutdown_task->manager_cmd.target_dir[0]    = '\0';
    //     shutdown_task->manager_cmd.cancel_dir[0]    = '\0';

    //     enqueue_task(&queue_tasks, shutdown_task);
    // }

    printf("\n exiting \n");
    // for(int i = 0; i < worker_limit; i++){
    //     pthread_join(worker_th[i], NULL);
    // }
    
    
    print_queue(&queue_tasks);
    // printf("\n-----------------PRIORITY INSERT-----------------\n");

    // shift_queue(&queue_tasks);
    // print_queue(&queue_tasks);


    free(conf_pairs);
    // free(worker_th);
    close(socket_manager);
    close(socket_console_read);
    free(queue_tasks.tasks_array);
    free_all_sync_info(&sync_info_head);


    return 0;
}




static const char* cmd_to_str(manager_command cmd){
    if(cmd.op == ADD)       return "ADD";
    if(cmd.op == CANCEL)    return "CANCEL";
    if(cmd.op == SHUTDOWN)    return "SHUTDOWN";
    
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


// debugging functon before implementing actual priority insert
void shift_queue(sync_task_ts *task){

    for(int i = task->size - 1; i >= 0; i--){
        int old_idx = (task->head + i) % task->buffer_slots;
        int new_idx = (task->head + i + 1) % task->buffer_slots;

        task->tasks_array[new_idx] = task->tasks_array[old_idx];
    }
    sync_task *newtask = malloc(sizeof(sync_task));
    // random op
    newtask->manager_cmd.op = SHUTDOWN;

    task->tasks_array[task->head] = newtask;
    task->tail = (task->tail + 1) % task->buffer_slots;
    task->size++;


}