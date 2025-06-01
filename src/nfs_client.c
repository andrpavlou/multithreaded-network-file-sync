#define _GNU_SOURCE

#include "utils.h"
#include "common.h"
#include "common_defs.h"
#include "nfs_client_def.h"

/*
     socket()
        |
     bind()
        |
     listen()
        |
     accept()
        |
(wait for connection)
        |
      read()
        |
    (processing)
        |
      write()
*/


/*
    commands without replying to anyone, just console:

    echo "LIST ./test_dir" | nc localhost 2424
    
    echo "PULL ./test_dir/test.txt" | nc localhost 2424
    
    echo "PUSH ./test_dir/test.txt -1 Hello world" | nc localhost 2424
    echo "PUSH" ./test_dir/test.txt 11 Hello world" | nc localhost 2424

*/
struct linux_dirent64 {
    ino64_t        d_ino;
    off64_t        d_off;
    unsigned short d_reclen;
    unsigned char  d_type;
    char           d_name[];
};

#define CHECK_READ(read_b, total_read, buffer)  \
    do {                                        \
        if((read_b) == 0){                      \
            buffer[total_read] = '\0';          \
            return total_read;                  \
        }                                       \
        if((read_b) < 0) return -1;             \
    }while(0) 


ssize_t read_line(int socket, char *buffer, ssize_t max_read){
    char current_char;
    ssize_t total_read = 0;

    while(total_read < max_read - 1){
        ssize_t read_b = read(socket, &current_char, 1);
        CHECK_READ(read_b, total_read, buffer);

        buffer[total_read++] = current_char;
        
        if(current_char == '\r'){
            // Found \r so read one more byte to see if the pattern \r\n if found
            read_b = read(socket, &current_char, 1);
            CHECK_READ(read_b, total_read, buffer);

            if(current_char == '\n'){
                // Pattern found so just remove \r
                buffer[total_read - 1] = '\0';
                return total_read - 1;
            }

            // \r\n not found yet, just keep going
            buffer[total_read++] = current_char;
        }
    }

    buffer[total_read] = '\0';
    return total_read;
}



volatile sig_atomic_t client_active = 1;


int arg_check(int argc, const char** argv, int *port){
    if(argc != 3) return 1;

    if(strncmp(argv[1], "-p", 2)) return 1;

    *port = atoi(argv[2]);
    return 0;
}


const char* cmd_to_str(client_operation cmd){
    if(cmd == LIST) return "LIST";
    if(cmd == PULL) return "PULL";
    if(cmd == PUSH) return "PUSH";
    
    return "INVALID";
}


int parse_manager_command(const char* buffer, client_command *full_command){
    memset(full_command->data, 0, BUFFSIZ);

    if(!strncmp(buffer, "LIST", 4)){
        full_command->op = LIST;
        strncpy(full_command->path, &buffer[5], BUFFSIZ - 1);
        full_command->path[BUFFSIZ - 1] = '\0';
        
        return 0;
    }

    if(!strncmp(buffer, "PULL", 4)) {
        full_command->op = PULL;
        strncpy(full_command->path, &buffer[5], BUFFSIZ - 1);
        full_command->path[BUFFSIZ - 1] = '\0';
        
        return 0;
    } 

    if(!strncmp(buffer, "PUSH", 4)){
        full_command->op = PUSH;

        char tmp_buffer[2 * BUFFSIZ];
        strncpy(tmp_buffer, buffer, sizeof(tmp_buffer) - 1);
        tmp_buffer[sizeof(tmp_buffer) - 1] = '\0';

        char *token = strtok(tmp_buffer, " "); 
        token = strtok(NULL, " ");       
        if(!token) return 1;
        

        strncpy(full_command->path, token, BUFFSIZ - 1);
        full_command->path[BUFFSIZ - 1] = '\0';
        
        token = strtok(NULL, " ");       
        if(!token) return 1;


        full_command->chunk_size = atoi(token);
        if(full_command->chunk_size < -1 || full_command->chunk_size > BUFFSIZ){
            perror("invalid chunk size sent");
            return 1;
        } 


        /* 
        * Will not copy full_command->data right now it will be read directly
        * in the exec command as the protocol is \r\n, so first read the command 
        * get the batch size then expect it 
        */
        if(full_command->chunk_size == -1){
            full_command->data[0] = '\0';
            return 0;
        }
        
        full_command->data[strlen(token) + 1] = '\0';
        return 0;
    }
    return 1;
}



int get_filenames(const char *path, char filenames[][BUFFSIZ]){
    char buf[BUFFSIZ];
    
    int dir_fd = open(path, O_RDONLY | O_DIRECTORY);
    if(dir_fd == -1) {
        perror("open dir");
        return -1;
    }

    int nread       = 0;
    int file_count  = 0;
    while((nread = getdents64(dir_fd, buf, BUFFSIZ)) > 0){
        int bpos = 0;
        while(bpos < nread){
            struct linux_dirent64 *dir = (struct linux_dirent64 *)(buf + bpos);
            
            // Skip previous dir and pwd
            if(strcmp(dir->d_name, ".") && strcmp(dir->d_name, "..")){
                // printf("dir name: %s\n", dir->d_name);
                strncpy(filenames[file_count], dir->d_name, BUFFSIZ - 1);
                filenames[file_count][BUFFSIZ - 1] = '\0';

                file_count++;
                if(file_count >= MAX_FILES) return 1;
            }
            bpos += dir->d_reclen;
        }
    }

    if(nread == -1){
        perror("getdents64");
        return -1;
    }

    close(dir_fd);
    return file_count;
}



int exec_command(client_command cmd, int newsock){
    if(cmd.op == LIST){
        char clean_path[BUFFSIZ];
        strncpy(clean_path, cmd.path, BUFFSIZ - 1);
        // '\0' if there is newline in the path to ignore it
        clean_path[BUFFSIZ - 1] = '\0';
        clean_path[strcspn(clean_path, "\n")] = '\0';
        
        char src_files[MAX_FILES][BUFFSIZ];
        int src_files_count = get_filenames(clean_path, src_files);

        if(src_files_count == -1) return -1;

        for(int i = 0; i < src_files_count; i++){
            char output_buff[BUFFSIZ];
            int len = snprintf(output_buff, sizeof(output_buff), "%s\n", src_files[i]);                
            write(newsock, output_buff, len);
        }
        write(newsock, ".\n", 2);

        return 0;
    }

    if(cmd.op == PULL){
        int fd_file_read = 0;
        if((fd_file_read = open(cmd.path, O_RDONLY)) == -1){
            char error_msg[] = "-1\n";

            write_all(newsock, error_msg, strlen(error_msg));
            close(fd_file_read);
            
            return -1;
        }
        
        off_t file_size = lseek(fd_file_read, 0, SEEK_END);
        lseek(fd_file_read, 0, SEEK_SET);

        char size_buf[BUFFSIZ / 4];
        int len = snprintf(size_buf, sizeof(size_buf), "%ld ", file_size);
        if(write_all(newsock, size_buf, len) == -1){
            perror("Write size");
            close(fd_file_read);
            
            return -1;
        }
        ssize_t read_bytes;
        char buffer[BUFFSIZ];
        while((read_bytes = read(fd_file_read, buffer, sizeof(buffer))) > 0){
            if(write_all(newsock, buffer, read_bytes) == -1){
                perror("Write full");
                
                close(fd_file_read);
                return -1;;
            }
        }

        close(fd_file_read);
        return 0;
    }

    if(!cmd.chunk_size && cmd.op == PUSH) return 0; // final chunk, nothing to write

    // just create the file if chunk size is -1
    if(cmd.op == PUSH && cmd.chunk_size == -1){
        int fd_file_write = open(cmd.path, O_WRONLY | O_TRUNC | O_CREAT, 0666);
        
        printf("FILENAME: %s\n", cmd.path);
        if(fd_file_write == -1){
            perror("push open\n");
            return 1;
        }
        // if(write(fd_file_write, cmd.data, 0) < 0){
        //     perror("Invalid written bytes");
        //     return 1;
        // }
        close(fd_file_write);
        return 0;
    }

    if(cmd.op == PUSH){
        // Read the data becasue we send: PUSH file size\r\ndata
        // important because data might have \r\n in them
        if(read_all(newsock, cmd.data, cmd.chunk_size) < 0){
            perror("read exec command for push");
            return 1;
        }
        int fd_file_write = open(cmd.path, O_WRONLY | O_APPEND);
        
        if(fd_file_write == -1){
            perror("push open\n");
            return 1;
        }


        ssize_t write_bytes = write(fd_file_write, cmd.data, cmd.chunk_size);
        if(write_bytes != cmd.chunk_size){
            perror("Invalid written bytes");
            return 1;
        }

        close(fd_file_write);
        return  0;
    }
    
    // Invalid command
    return 1;
}

void handle_sigint(int sig){
    client_active = 0;
}




void* handle_connection(void* arg){
    int newsock = *(int*)arg;
    free(arg);

    ssize_t n_read;
    char read_buffer[BUFFSIZ];
    memset(read_buffer, 0, sizeof(read_buffer));

    while((n_read = read_line(newsock, read_buffer, sizeof(read_buffer))) > 0){
        printf("Got line: [%s] Size [%ld]\n", read_buffer, n_read);
        fflush(stdout);

        client_command current_cmd_struct;
        if(parse_manager_command(read_buffer, &current_cmd_struct)){
            fprintf(stderr, "error: parse_command [%s]\n", read_buffer);
            break;
        }

        if(exec_command(current_cmd_struct, newsock)){
            perror("exec command");
            break;
        }
        memset(read_buffer, 0, sizeof(read_buffer));
    }

    close(newsock);
    return NULL;
}




// lsof -i :port
int main(int argc, char* argv[]){
    int port = 0;   
    if(arg_check(argc, (const char**)argv, &port)){
        perror("USAGE: ./nfs_client -p <port_number>");
        return 1;
    }

    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; 
    sigaction(SIGINT, &sa, NULL);


    struct sockaddr_in server, client;
    struct sockaddr *serverptr = (struct sockaddr *) &server;
    struct sockaddr *clientptr = (struct sockaddr *) &client;
    

    int sock = 0;
    if((sock = socket(AF_INET , SOCK_STREAM , 0)) < 0)
        perror("socket");
    
    int optval = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    server.sin_family       = AF_INET ; /* Internet domain */
    server.sin_addr.s_addr  = htonl(INADDR_ANY);
    server.sin_port         = htons(port) ; /* The given port */
    
    
    /* Bind socket to address */
    if(bind(sock, serverptr, sizeof(server)) < 0){
        perror("bind");
        exit(1);
    }

    if(listen(sock, 5) < 0)
        perror (" listen ") ;
    
    printf("Listening for connections to port % d \n", port);
    socklen_t clientlen;
    while(client_active){
        clientlen = sizeof(struct sockaddr_in);
        int *newsock = malloc(sizeof(int));

        if((*newsock = accept(sock, clientptr, &clientlen)) < 0){
            perror("accept ");
            continue;
        }
        printf("Accepted connection \n") ;

        pthread_t client_th;
        if(pthread_create(&client_th, NULL, handle_connection, newsock) != 0){
            perror("pthread_create");
            
            close(*newsock);
            free(newsock);
            continue;
        }

        pthread_detach(client_th);
    }
    close(sock);

    printf("Exiting\n");
    return 0;
}



// printf("Parsed Command: %s\n", cmd_to_str(cmd.op));
//                 printf("Directory Path: %s\n", cmd.path);

//                 if(cmd.op == PUSH) {
//                     printf("chunk size: %d\n", cmd.chunk_size);
//                     printf("Data: %s\n", cmd.data);
//                 }
