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
int write_all(int fd, const void *buff, size_t size) {
    int sent, n;
    for(sent = 0; sent < size; sent+=n) {
        if ((n = write(fd, buff+sent, size-sent)) == -1) return -1;
    }

    return sent;
}

int parse_pull_read(const char *rec_buff, ssize_t *rec_file_len, char **file_content){
    char temp_buff[BUFSIZ];
    strncpy(temp_buff, rec_buff, sizeof(temp_buff) - 1);
    temp_buff[sizeof(temp_buff) - 1] = '\0';

    char *token = strtok(temp_buff, " ");
    if(!token) return 1;

    *rec_file_len = atoi(token);
    if(*rec_file_len == -1){
        perror("File not found from client");
        return 1;
    }

    char *first_space = strchr(rec_buff, ' ');
    if(first_space == NULL) return 1;

    *file_content = first_space + 1;

    return 0;
}

void *worker_thread(void *arg){
    config_pairs* conf_pairs = (config_pairs*) arg;
    struct sockaddr_in servadd; /* The address of server */
    struct hostent *hp; /* to resolve server ip */
    int sock, n_read;

    if((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        perror( "socket" );
    
    if((hp = gethostbyname(conf_pairs->source_ip)) == NULL) {
        perror("gethostbyname"); 
        exit(1);
    }

    memcpy(&servadd.sin_addr, hp->h_addr, hp->h_length);
    servadd.sin_port = htons(conf_pairs->source_port); /* set port number */
    servadd.sin_family = AF_INET ; /* set socket type */

    if(connect(sock, (struct sockaddr*) &servadd, sizeof(servadd)) !=0)
        perror("connect");
    
    ////////////// PULL COMMAND //////////////

    // Request pull of given text TODO: make it dynamic with list
    char pull_cmd_buff[BUFFSIZ * 4];
    memset(pull_cmd_buff, 0, strlen(pull_cmd_buff));

    snprintf(pull_cmd_buff, sizeof(pull_cmd_buff), "PULL %s/test.txt", conf_pairs->source_dir_path);
    if(write_all(sock, pull_cmd_buff, strlen(pull_cmd_buff)) == -1)
        perror("write");

    
    char num_buff[32];
    char space_ch;
    int ind = 0;

    while(read(sock, &space_ch, 1) == 1){
        if(ch == ' ')
            break;
        
        num_buff[ind++] = space_ch;
    }
    num_buff[ind] = '\0';
    long file_size = strtol(num_buff, NULL, 10);

    printf("CONTENT: ");
    char pull_buffer[BUFFSIZ];
    ssize_t total_read = 0;
    while(total_read < file_size){
        int request_bytes = file_size - total_read < BUFFSIZ ? file_size - total_read : BUFFSIZ;

        n_read = read(sock, pull_buffer, request_bytes);
        if(n_read < request_bytes){
            perror("read");
        }

        pull_buffer[n_read] = '\0';
        printf("%s", pull_buffer);
        
        total_read += n_read;
        memset(pull_buffer, 0, sizeof(pull_buffer)); 
    }

    // Read response of pull command 
    // char pull_response_buff[BUFFSIZ];
    // if((n_read = read(sock, pull_response_buff, sizeof(pull_response_buff))) <= 0){
    //     perror("pull read");
    //     close(sock);
    //     return NULL;
    // }
    // pull_response_buff[n_read] = '\0';
    // close(sock);
    
    // // Parse response of pull command 
    // ssize_t rec_file_len    = 0;
    // char *file_content      = NULL;
    
    // int status = parse_pull_read(pull_response_buff, &rec_file_len, &file_content);
    // if(status){
    //     perror("parse pull");
    //     close(1);
    // }

    // printf("SIZE: %ld\n", rec_file_len);
    // printf("CONTENT:[%s]\n", file_content);
    
    ////////////// PUSH COMMAND //////////////

    // Open new connection for target to push data
    // if((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    //     perror( "socket" );
    
    // if((hp = gethostbyname(conf_pairs->target_ip)) == NULL) {
    //     perror("gethostbyname"); 
    //     exit(1);
    // }

    // memcpy(&servadd.sin_addr, hp->h_addr, hp->h_length);
    // servadd.sin_port = htons(conf_pairs->target_port); /* set port number */
    // servadd.sin_family = AF_INET ; /* set socket type */

    // // First connection to create the file, must weather we need to do this with -1 on first run and then send it.
    // if(connect(sock, (struct sockaddr*) &servadd, sizeof(servadd)) !=0)
    //     perror("connect");
    
    // // Creates file on remote host
    // char push_cmd_buff[BUFFSIZ * 4];
    // snprintf(push_cmd_buff, sizeof(push_cmd_buff), "PUSH %s/test.txt %d %s", 
    //     conf_pairs->target_dir_path,
    //     FILE_NF_LEN, 
    //     "");

    // if(write_all(sock, push_cmd_buff, strlen(push_cmd_buff)) == -1)
    //     perror("fwrite");

        
    // if((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    //     perror( "socket" );
    
    // if((hp = gethostbyname(conf_pairs->target_ip)) == NULL) {
    //     perror("gethostbyname"); 
    //     exit(1);
    // }

    // memcpy(&servadd.sin_addr, hp->h_addr, hp->h_length);
    // servadd.sin_port = htons(conf_pairs->target_port); /* set port number */
    // servadd.sin_family = AF_INET ; /* set socket type */

    // if(connect(sock, (struct sockaddr*) &servadd, sizeof(servadd)) !=0)
    //     perror("connect");
    
    
    // // Send the actual text...
    // memset(push_cmd_buff, 0, sizeof(push_cmd_buff));
    // snprintf(push_cmd_buff, sizeof(push_cmd_buff), "PUSH %s/test.txt %ld %s", 
    //     conf_pairs->target_dir_path,
    //     rec_file_len, 
    //     file_content);        



    // if(write_all(sock, push_cmd_buff, strlen(push_cmd_buff)) == -1)
    //     perror("fwrite");

    
    // close(sock);

    return NULL;
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

    printf("logfile: %s\n", logfile);
    printf("config_file: %s\n", config_file);
    printf("worker_limit: %d\n", worker_limit);
    printf("port: %d\n", port);
    printf("buffer_size: %d\n", buffer_size);

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
    
    pthread_t worker_th;
    pthread_create(&worker_th, NULL, worker_thread, &conf_pairs[0]);
    pthread_join(worker_th, NULL);


    return 0;
}