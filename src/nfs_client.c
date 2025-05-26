#include "nfs_client.h"


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

        token = strtok(NULL, "");        
        if(!token) return 1;

        size_t memcpy_s = full_command->chunk_size == -1 ? strlen(token) : full_command->chunk_size;
        memcpy(full_command->data, token, memcpy_s);
        full_command->data[strlen(token) + 1] = '\0';

        return 0;
    }
    return 1;
}



int get_filenames(const char *path, char filenames[][BUFFSIZ]){
    printf("PATH [%s]\n\n", path);

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
            
            // Skip .. and .
            if(strcmp(dir->d_name, ".") && strcmp(dir->d_name, "..")){
                printf("dir name: %s\n", dir->d_name);
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



int exec_command(const client_command cmd, int newsock){
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

    if(cmd.op == PUSH){
        int fd_file_write = 0;
        fd_file_write = cmd.chunk_size == -1 ?  open(cmd.path, O_WRONLY | O_TRUNC | O_CREAT, 0666) : 
                                                open(cmd.path, O_WRONLY | O_APPEND);
        
        if(fd_file_write == -1){
            perror("push open\n");
            return 1;
        }

        int actual_write_len = cmd.chunk_size == -1 ? strlen(cmd.data) : cmd.chunk_size;

        ssize_t write_bytes;
        write_bytes = write(fd_file_write, cmd.data, actual_write_len);

        if(write_bytes != actual_write_len){
            perror("Invalid written bytes");
            return 1;
        }

        close(fd_file_write);
        return  0;
    }

    return 1;
}

void handle_sigint(int sig){
    client_active = 0;
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

    server.sin_family = AF_INET ; /* Internet domain */
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port) ; /* The given port */
    /* Bind socket to address */


    if(bind(sock, serverptr, sizeof(server)) < 0){
        perror("bind");
        exit(1);
    }

    if(listen(sock, 5) < 0)
        perror (" listen ") ;
    
    socklen_t clientlen;
    printf("Listening for connections to port % d \n", port);
    int newsock = 0;
    
    while(client_active){
        clientlen = sizeof(struct sockaddr_in);
        if((newsock = accept(sock, clientptr, &clientlen)) < 0){
            perror("accept ");
            continue;
        }

        printf("Accepted connection \n") ;
        ssize_t n_read;
        char read_buffer[BUFFSIZ];
        memset(read_buffer, 0, sizeof(read_buffer));
        while((n_read = read(newsock, read_buffer, sizeof(read_buffer))) > 0){
            read_buffer[n_read] = '\0';
            
            client_command current_cmd_struct;
            if(parse_manager_command(read_buffer, &current_cmd_struct)){
                fprintf(stderr, "error: parse_command [%s]\n", read_buffer);
                break;
            }

            printf("Parsed Command: %s\n", cmd_to_str(current_cmd_struct.op));
            printf("Directory Path: %s\n", current_cmd_struct.path);

            if(current_cmd_struct.op == PUSH) {
                printf("chunk size: %d\n", current_cmd_struct.chunk_size);
                printf("Data: %s\n", current_cmd_struct.data);
            }

            if(exec_command(current_cmd_struct, newsock)){
                perror("exec command");
                break;
            }
            write(newsock, "ACK\n", 4);

            memset(read_buffer, 0, sizeof(read_buffer));
        }

        
        close(newsock);
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
