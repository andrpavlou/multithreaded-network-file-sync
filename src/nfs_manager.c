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

volatile sig_atomic_t manager_active = 1;

void *worker_thread(void *arg){
    config_pairs* conf_pairs = (config_pairs*) arg;
    int sock_source_read;
    if(establish_connection(&sock_source_read, conf_pairs->source_ip, conf_pairs->source_port)){
        perror("establish con");
        exit(1);
    }   

    char list_cmd_buff[BUFFSIZ];
    int list_len = snprintf(list_cmd_buff, sizeof(list_cmd_buff), "LIST %s", conf_pairs->source_dir_path);
    list_cmd_buff[list_len] = '\0';


    //========== Request List Command ========== //
    if(write_all(sock_source_read, list_cmd_buff, list_len) == -1){
        perror("list write");
        return NULL;
    }

    char *list_reply_buff = NULL;
    read_list_response(sock_source_read, &list_reply_buff);
    
    // Locate ack and remove it
    char *ack_start = strstr(list_reply_buff, "\nACK\n");
    if(ack_start) ack_start[0] = '\0'; 

    char *file_buff[MAX_FILES];
    int total_src_files = get_files_from_list_response(list_reply_buff, file_buff);

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

        memset(pull_request_cmd_buff, 0, sizeof(pull_request_cmd_buff));
        int len_pull_req = snprintf(pull_request_cmd_buff, sizeof(pull_request_cmd_buff), 
                            "PULL %s/%s", 
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
        while(total_read_pull_req < file_size){
            ssize_t request_bytes = (file_size - total_read_pull_req) < BUFFSIZ ? (file_size - total_read_pull_req) : BUFFSIZ - 2;

            ssize_t const_extra_bytes = strlen("PUSH") + strlen(file_buff[i]) + 1 + 
                            BUFFSIZ_CHARS  + strlen(conf_pairs->target_dir_path) + 1 + 2; // 2 spaces + 1 "/"

            request_bytes -= request_bytes + const_extra_bytes > BUFFSIZ ? const_extra_bytes : 0;

            // Request_bytes = 0 -> just send push request with chunk size = 0 indicating the last push
            if(!request_bytes){
                int push_len = snprintf(push_buffer, sizeof(push_buffer), "PUSH %s/%s %ld %s", 
                                    conf_pairs->target_dir_path,
                                    file_buff[i],
                                    request_bytes,
                                    pull_buffer);

                if(write_all(sock_target_push, push_buffer, push_len) == -1){
                    perror("push write");
                }
                
                if(receive_ack(sock_target_push)) break;
            }
            
            ssize_t n_read;
            if((n_read = read_all(sock_source_read, pull_buffer, request_bytes)) <= 0){
                perror("\nread pull_buffer");
                break;
            }
            pull_buffer[n_read] = '\0';
            
            /*
            * Send -1 batch size to create the file if it's the first write,
            * otherwise send the bytes read from other host -> request_bytes
            */
            int write_batch_bytes = first_push_write == 0 ? -1 : request_bytes; 
            int push_len = snprintf(push_buffer, sizeof(push_buffer), "PUSH %s/%s %d %s", 
                                conf_pairs->target_dir_path,
                                file_buff[i],
                                write_batch_bytes,
                                pull_buffer);

            if(write_all(sock_target_push, push_buffer, push_len) == -1) {
                perror("write push");
            }

            first_push_write = 1;
            if(receive_ack(sock_target_push)) break;


            total_read_pull_req += n_read;
            memset(pull_buffer, 0, sizeof(pull_buffer)); 
            memset(push_buffer, 0, sizeof(push_buffer)); 
        }

        if(receive_ack(sock_source_read)) break;
    }

    close(sock_source_read);
    close(sock_target_push);
    free(list_reply_buff);
    
    return NULL;
}

void handle_sigint(int sig){
    manager_active = 0;
}


// bin/nfs_manager -l logs/manager.log -c config.txt -n 5 -p 2345 -b 512
int main(int argc, char *argv[]){
    char *logfile       = NULL;
    char *config_file   = NULL;
    
    int port            = -1;
    int worker_limit    = -1;
    int buffer_size     = -1;

    if(check_args_manager(argc, argv, &logfile, &config_file, &worker_limit, &port, &buffer_size)){
        perror("USAGE: ./nfs_manager -l <logfile> -c <config_file> -n <worker_limit> -p <port> -b <buffer_size>");
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
    
    int socket_client = 0;
    clientlen = sizeof(struct sockaddr_in);
    if((socket_client = accept(socket_manager, clientptr, &clientlen)) < 0){
        perror("accept ");
        return 1;
    }

    printf("Accepted connection \n") ;
    while(manager_active){

        char read_buffer[BUFFSIZ];
        memset(read_buffer, 0, sizeof(read_buffer));
        
        ssize_t n_read;
        
        if((n_read = read(socket_client, read_buffer, BUFFSIZ - 1)) <= 0){
            perror("Read from console");
            return 1;
        }
        read_buffer[n_read] = '\0';

        printf("Read %s\n", read_buffer);
    }

    close(socket_client);
    close(socket_manager);

    printf("\n exiting \n");


    
    ////// Thread workers for tasks
    // pthread_t worker_th;
    // pthread_create(&worker_th, NULL, worker_thread, &conf_pairs[0]);
    // pthread_join(worker_th, NULL);


    return 0;
}