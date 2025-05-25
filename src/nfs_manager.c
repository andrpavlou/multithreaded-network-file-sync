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
ssize_t write_all(int fd, const void *buff, size_t size) {
    ssize_t sent, n;
    for(sent = 0; sent < size; sent+=n) {
        if ((n = write(fd, buff+sent, size-sent)) == -1) return -1;
    }

    return sent;
}

ssize_t read_all(int fd, void *buf, size_t size){
    ssize_t total_read_b = 0;
    ssize_t n = 0;
    for( ; total_read_b < size; total_read_b += n){
        n = read(fd, (char*) buf + total_read_b, size - total_read_b);
        
        if(n <= 0) return n;
    }
    return total_read_b;
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

int establish_connection(int *sock, const char *ip, const int port){
    struct sockaddr_in servadd; /* The address of server */
    struct hostent *hp;         /* to resolve server ip */

    if((*sock = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("socket");
        return 1;
    }
    
    if((hp = gethostbyname(ip)) == NULL){
        perror("gethostbyname"); 
        return 1;
    }

    memcpy(&servadd.sin_addr, hp->h_addr, hp->h_length);
    servadd.sin_port = htons(port); /* set port number */
    servadd.sin_family = AF_INET;   /* set socket type */

    if(connect(*sock, (struct sockaddr*) &servadd, sizeof(servadd)) != 0){
        perror("connect");
        return 1;
    }
    return 0;
}

int read_list_response(int sock, char **list_reply_buff){
    size_t total_read_list      = 0;
    size_t list_buff_capacity   = 0;

    do{
        // Allocate more memory for buffer if there is not enough space
        if(list_buff_capacity - total_read_list < BUFFSIZ){
            list_buff_capacity += BUFFSIZ;
            *list_reply_buff = realloc(*list_reply_buff, list_buff_capacity);

            if(*list_reply_buff == NULL){
                perror("Realloc");
                return 1;
            }
            (*list_reply_buff)[list_buff_capacity - 1] = '\0';
        }
        ssize_t n_read = read(sock, *list_reply_buff + total_read_list, BUFFSIZ);
        if(n_read <= 0){
            perror("read");
            return 1;
        }

        total_read_list += n_read;
        (*list_reply_buff)[total_read_list] = '\0';
    } while(strstr((*list_reply_buff), ".\nACK\n") == NULL); // Keep reading until we receive '.\nACK\n'

    return 0;
}

int get_files_from_list_response(char *list_reply_buff, char *file_buff[MAX_FILES]){
    int total_src_files = 0;
    char *curr_file = strtok(list_reply_buff, "\n");
    
    file_buff[total_src_files] = curr_file;
    while(curr_file && strcmp(curr_file, ".") != 0 && total_src_files < MAX_FILES){
        total_src_files++;
        
        curr_file = strtok(NULL, "\n");
        file_buff[total_src_files] = curr_file;
    }

    return total_src_files;
}

long get_file_size_of_host(int sock){
    int ind = 0;
    char space_ch;
    char num_buff[32];

    // Read the size of the file we are about to send
    while(read(sock, &space_ch, 1) == 1){
        if(space_ch == ' ') break;
        
        num_buff[ind++] = space_ch;
    }
    num_buff[ind] = '\0';

    char *end;
    long file_size = strtol(num_buff, &end, 10);
    if(end == num_buff || *end != '\0') return -1;

    return file_size;
}   


int receive_ack(int sock){
    char ack[8] = {0};
    
    int ack_read = read(sock, ack, sizeof(ack) - 1);
    if(ack_read <= 0 || strncmp(ack, "ACK", 3)){
        fprintf(stderr, "No ACK REC: [%s]\n", ack);
        return 1;
    }
    
    return 0;
}


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

    // printf("logfile: %s\n", logfile);
    // printf("config_file: %s\n", config_file);
    // printf("worker_limit: %d\n", worker_limit);
    // printf("port: %d\n", port);
    // printf("buffer_size: %d\n", buffer_size);

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