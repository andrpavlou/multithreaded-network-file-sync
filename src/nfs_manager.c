#include "nfs_manager.h"
#include "utils.h"
#include "sync_task.h"
#include "sync_info.h"


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

volatile sig_atomic_t manager_active = 1;


void handle_sigint(int sig){
    manager_active = 0;
}



void *worker_thread(void *arg){
    config_pairs* conf_pairs = (config_pairs*) arg;
    int sock_source_read;
    if(establish_connection(&sock_source_read, conf_pairs->source_ip, conf_pairs->source_port)){
        perror("establish con");
        exit(1);
    }   

    char list_cmd_buff[BUFFSIZ];
    int list_len = snprintf(list_cmd_buff, sizeof(list_cmd_buff), "LIST %s\r\n", conf_pairs->source_dir_path);
    list_cmd_buff[list_len] = '\0';


    //========== Request List Command ========== //
    if(write_all(sock_source_read, list_cmd_buff, list_len) == -1){
        perror("list write");
        return NULL;
    }

    char *list_reply_buff = NULL;
    read_list_response(sock_source_read, &list_reply_buff);
    printf("response: %s\n", list_reply_buff);
    

    
    char *file_buff[MAX_FILES];
    memset(file_buff, 0, sizeof(file_buff));
    
    int total_src_files = get_files_from_list_response(list_reply_buff, file_buff);
    printf("\ntotal_files %d\nfile0:%s\nfile1:%s\nfile2:%s\n", total_src_files, file_buff[0], file_buff[1], file_buff[2]);
    
    // Establish connection with the target host to sync the files with pushes
    int sock_target_push;
    if(establish_connection(&sock_target_push, conf_pairs->target_ip, conf_pairs->target_port)){
        perror("establish con");
        exit(1);
    }   
    
    char pull_buffer[BUFFSIZ];
    char push_buffer[BUFFSIZ];
    char pull_request_cmd_buff[BUFFSIZ];
    
    for(int i = 0; i < total_src_files; i++){
        unsigned short first_push_write = 0;
        printf("\ni = %d\n", i);

        memset(pull_request_cmd_buff, 0, sizeof(pull_request_cmd_buff));
        int len_pull_req = snprintf(pull_request_cmd_buff, sizeof(pull_request_cmd_buff), 
                            "PULL %s/%s\r\n", 
                            conf_pairs->source_dir_path,
                            file_buff[i]);
        
        // Request pull of given text
        if(write_all(sock_source_read, pull_request_cmd_buff, len_pull_req) == -1){
            perror("pull write");
            return NULL;
        }
        
        long file_size = 0;
        if((file_size = get_file_size_of_host(sock_source_read)) == -1){
            perror("get_file_size_of_host");
            return NULL;
        }
        printf("file size: %ld\n", file_size);
        
        
        memset(pull_buffer, 0, sizeof(pull_buffer));
        memset(push_buffer, 0, sizeof(push_buffer));
        
        ssize_t total_read_pull_req = 0;
        // Read response of pull command 
        while(total_read_pull_req <= file_size){
            ssize_t request_bytes = (file_size - total_read_pull_req) < BUFFSIZ 
                                    ? (file_size - total_read_pull_req) 
                                    : BUFFSIZ - 1;


            // Request_bytes = 0 -> just send push request with chunk size = 0 indicating the last push
            if(!request_bytes){
                int push_len = snprintf(push_buffer, sizeof(push_buffer), "PUSH %s/%s %d\r\n", 
                                    conf_pairs->target_dir_path,
                                    file_buff[i],
                                    0);

                if(write_all(sock_target_push, push_buffer, push_len) == -1){
                    perror("push write");
                }
                break;
            }
            
            ssize_t n_read;
            if((n_read = read_all(sock_source_read, pull_buffer, request_bytes)) <= 0){
                perror("\nread pull_buffer");
                break;
            }
            // pull_buffer[n_read] = '\0';
            

            /*
            * Send -1 batch size to create the file if it's the first write,
            * otherwise send the bytes read from other host -> request_bytes
            */
            char push_header_buffer[BUFFSIZ];
            int write_batch_bytes = first_push_write == 0 ? -1 : request_bytes; 
            int push_header_len = snprintf(push_header_buffer, sizeof(push_header_buffer), "PUSH %s/%s %d\r\n", 
                                conf_pairs->target_dir_path,
                                file_buff[i],
                                write_batch_bytes);

            if(write_all(sock_target_push, push_header_buffer, push_header_len) == -1){
                perror("write push");
            }
            if(!first_push_write){
                push_header_len = snprintf(push_header_buffer, sizeof(push_header_buffer), "PUSH %s/%s %ld\r\n", 
                                    conf_pairs->target_dir_path,
                                    file_buff[i],
                                    request_bytes);


                if(write_all(sock_target_push, push_header_buffer, push_header_len) == -1){
                    perror("write push");
                }
                if(write_all(sock_target_push, pull_buffer, request_bytes) == -1){
                    perror("write push");
                }

            } else {
                if(write_all(sock_target_push, pull_buffer, request_bytes) == -1){
                    perror("write push");
                }
            }

            first_push_write = 1;
            total_read_pull_req += n_read;

            memset(pull_buffer, 0, sizeof(pull_buffer)); 
            memset(push_buffer, 0, sizeof(push_buffer)); 
        }

    }

    close(sock_source_read);
    close(sock_target_push);
    free(list_reply_buff);
    
    return NULL;
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
        perror("list write");
        return 1;
    }

    char *list_reply_buff = NULL;
    read_list_response(sock_source_read, &list_reply_buff);


    char *file_buff[MAX_FILES];
    unsigned int total_src_files = get_files_from_list_response(list_reply_buff, file_buff);

    
    for(int i = 0; i < total_src_files; i++){
        sync_task *new_task         = malloc(sizeof(sync_task));
        new_task->manager_cmd.op    = curr_cmd.op;

        ssize_t manager_buff_size   = sizeof(new_task->manager_cmd.source_ip);

        // copy srouce ip/port
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
        if((find_node = find_sync_info((*sync_info_head), source_full_path)) != NULL){
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

    close(sock_source_read);
    return 0;
}



int enqueue_cancel_cmd(const manager_command curr_cmd, sync_task_ts *queue_tasks, sync_info_mem_store **sync_info_head,const char* source_full_path){
    int count = 0;
    char **found_hosts  = find_sync_info_hosts_by_dir((*sync_info_head), curr_cmd.cancel_dir, &count);
    if(!count) return 1;
    
    printf("\n--------------------\n");
    for(int i = 0; i < count; i++){
        int current_parsed_port = -1;
        char current_parsed_ip[BUFFSIZ];

        int status = parse_path(found_hosts[i], NULL, current_parsed_ip, &current_parsed_port);
        if(status){
            perror("parse path for cancel");
            continue;
        }

        sync_task *new_task         = malloc(sizeof(sync_task));
        new_task->manager_cmd.op    = curr_cmd.op;

        ssize_t manager_buff_size   = sizeof(new_task->manager_cmd.cancel_dir);

        // Copy cancelling dir
        strncpy(new_task->manager_cmd.cancel_dir, curr_cmd.cancel_dir, manager_buff_size - 1);
        new_task->manager_cmd.cancel_dir[manager_buff_size - 1] = '\0';

        // Copy source ip of the cancelling dir
        strncpy(new_task->manager_cmd.source_ip, current_parsed_ip, manager_buff_size - 1);
        new_task->manager_cmd.source_ip[manager_buff_size - 1] = '\0';
        
        // Copy source port of the cancelling dir
        new_task->manager_cmd.source_port = current_parsed_port ;

        sync_info_mem_store *curr_cancel_dir_node = find_sync_info((*sync_info_head), found_hosts[i]);
        new_task->node = curr_cancel_dir_node;

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







void *thread_exec_task(void *arg){
    sync_task_ts *queue_tasks   = (sync_task_ts*)  arg;
    sync_task *curr_task        = dequeue_task(queue_tasks);

    if(curr_task->manager_cmd.op == ADD){
        int sock_source_read;
        if(establish_connection(&sock_source_read, curr_task->manager_cmd.source_ip, curr_task->manager_cmd.source_port)){
            perror("establish con");
            exit(1);
        }   
        int sock_target_push;
        if(establish_connection(&sock_target_push, curr_task->manager_cmd.target_ip, curr_task->manager_cmd.target_port)){
            perror("establish con");
            exit(1);
        }   
        
        char pull_buffer[BUFFSIZ];
        char push_buffer[BUFFSIZ];
        char pull_request_cmd_buff[BUFFSIZ];
        
        unsigned short first_push_write = 0;

        memset(pull_request_cmd_buff, 0, sizeof(pull_request_cmd_buff));
        int len_pull_req = snprintf(pull_request_cmd_buff, sizeof(pull_request_cmd_buff), 
                            "PULL %s/%s\r\n",
                            curr_task->manager_cmd.source_dir,
                            curr_task->filename);
        
        // Request pull of given text
        if(write_all(sock_source_read, pull_request_cmd_buff, len_pull_req) == -1){
            perror("pull write");
            return NULL;
        }
        
        long file_size = 0;
        if((file_size = get_file_size_of_host(sock_source_read)) == -1){
            perror("get_file_size_of_host");
            return NULL;
        }
        
        
        memset(pull_buffer, 0, sizeof(pull_buffer));
        memset(push_buffer, 0, sizeof(push_buffer));
        
        ssize_t total_read_pull_req = 0;
        // Read response of pull command 
        while(total_read_pull_req <= file_size){
            ssize_t request_bytes = (file_size - total_read_pull_req) < BUFFSIZ 
                                    ? (file_size - total_read_pull_req) 
                                    : BUFFSIZ - 1;


            // Request_bytes = 0 -> just send push request with chunk size = 0 indicating the last push
            if(!request_bytes){
                int push_len = snprintf(push_buffer, sizeof(push_buffer), "PUSH %s/%s %d\r\n", 
                                    curr_task->manager_cmd.target_dir,
                                    curr_task->filename,
                                    0);

                if(write_all(sock_target_push, push_buffer, push_len) == -1){
                    perror("push write");
                }
                break;
            }
            
            ssize_t n_read;
            if((n_read = read_all(sock_source_read, pull_buffer, request_bytes)) <= 0){
                perror("\nread pull_buffer");
                break;
            }
            

            /*
            * Send -1 batch size to create the file if it's the first write,
            * otherwise send the bytes read from other host -> request_bytes
            */
            char push_header_buffer[BUFFSIZ];
            int write_batch_bytes = first_push_write == 0 ? -1 : request_bytes; 
            int push_header_len = snprintf(push_header_buffer, sizeof(push_header_buffer), "PUSH %s/%s %d\r\n", 
                                curr_task->manager_cmd.target_dir,
                                curr_task->filename,
                                write_batch_bytes);

            if(write_all(sock_target_push, push_header_buffer, push_header_len) == -1){
                perror("write push");
            }
            if(!first_push_write){
                push_header_len = snprintf(push_header_buffer, sizeof(push_header_buffer), "PUSH %s/%s %ld\r\n", 
                                    curr_task->manager_cmd.target_dir,
                                    curr_task->filename,
                                    request_bytes);


                if(write_all(sock_target_push, push_header_buffer, push_header_len) == -1){
                    perror("write push");
                }
                if(write_all(sock_target_push, pull_buffer, request_bytes) == -1){
                    perror("write push");
                }

            } else {
                if(write_all(sock_target_push, pull_buffer, request_bytes) == -1){
                    perror("write push");
                }
            }

            first_push_write = 1;
            total_read_pull_req += n_read;

            memset(pull_buffer, 0, sizeof(pull_buffer)); 
            memset(push_buffer, 0, sizeof(push_buffer)); 
        }
        close(sock_target_push);
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


        int status = enqueue_add_cmd(conf_cmd, &queue_tasks, &sync_info_head, conf_pairs[i].source_full_path, conf_pairs[i].target_full_path);
        if(status){
            perror("enqueue add");
        }
    }

    free(conf_pairs);
    pthread_t worker_th[6];
    for(int i = 0; i < 6; i++){
        pthread_create(&worker_th[i], NULL, thread_exec_task, &queue_tasks);
    }
    for(int i = 0; i < 6; i++){
        pthread_join(worker_th[i], NULL);
    }


    print_queue(&queue_tasks);


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

    if(listen(socket_manager, 5) < 0) perror ("listen") ;
    
    socklen_t clientlen;
    printf("Listening for connections to port % d \n", port);
    
    int socket_console_read = 0;
    clientlen = sizeof(struct sockaddr_in);
    if((socket_console_read = accept(socket_manager, clientptr, &clientlen)) < 0){
        perror("accept ");
        return 1;
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


        char *source_full_path;
        char *target_full_path;
        manager_command curr_cmd;
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

    // print_queue(&queue_tasks);

    
    // printf("\n exiting \n");
    
    ////// Thread workers for tasks
    // pthread_t worker_th[4];
    // for(int i = 0; i < 4; i++){
        // pthread_create(&worker_th[i], NULL, worker_thread, &conf_pairs[i]);
        // }
        // for(int i = 0; i < 4; i++){
            //     pthread_join(worker_th[i], NULL);
            // }
            
    
    close(socket_manager);
    close(socket_console_read);
    free(queue_tasks.tasks_array);
    free_all_sync_info(&sync_info_head);


    return 0;
}




static const char* cmd_to_str(manager_command cmd){
    if(cmd.op == ADD)       return "ADD";
    if(cmd.op == CANCEL)    return "CANCEL";
    
    return "INVALID";
}


void print_queue(sync_task_ts *task){
    if(!task){
        printf("NULL task\n");
        return;
    }
    

    for(int i = 0; i < task->size; i++){
        printf("--------- Sync Task: %d ---------\n", i);
        printf("Operation\t: %s\n", cmd_to_str(task->tasks_array[i]->manager_cmd));

        if(task->tasks_array[i]->manager_cmd.op == ADD){
            printf("Filename\t: %s\n", task->tasks_array[i]->filename );
            
            printf("Source Dir\t: %s\n", task->tasks_array[i]->manager_cmd.source_dir);
            printf("Source IP\t: %s\n", task->tasks_array[i]->manager_cmd.source_ip);
            printf("Source Port\t: %d\n", task->tasks_array[i]->manager_cmd.source_port);
            
            printf("Target Dir\t: %s\n", task->tasks_array[i]->manager_cmd.target_dir);
            printf("Target IP\t: %s\n", task->tasks_array[i]->manager_cmd.target_ip);
            printf("Target Port\t: %d\n", task->tasks_array[i]->manager_cmd.target_port);
        }

        if(task->tasks_array[i]->manager_cmd.op == CANCEL){
            printf("Source Dir\t: %s\n", task->tasks_array[i]->manager_cmd.cancel_dir);
            printf("Source IP\t: %s\n", task->tasks_array[i]->manager_cmd.source_ip);
            printf("Source Port\t: %d\n", task->tasks_array[i]->manager_cmd.source_port);
        }


        if(task->tasks_array[i]->node){
            printf("Sync Info:\n");
            printf("  Source\t: %s\n", task->tasks_array[i]->node->source);
            printf("  Target\t: %s\n", task->tasks_array[i]->node->target);
        } else {
            printf("Sync Info: NULL\n");
        }

        printf("----------------------------------\n");
    }

    
}
