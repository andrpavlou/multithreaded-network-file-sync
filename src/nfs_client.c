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

    echo "LIST" ./test_dir | nc localhost 2324
    
    echo "PULL" ./test_dir/test.txt" | nc localhost 2324
    
    echo "PUSH" ./test_dir/test.txt -1 Hello world" | nc localhost 2324
    echo "PUSH" ./test_dir/test.txt 11 Hello world" | nc localhost 2324

*/

volatile sig_atomic_t client_active = 1;


int arg_check(int argc, const char** argv, int *port){
    if(argc != 3) return 1;

    if(strncmp(argv[1], "-p", 2)) return 1;

    *port = atoi(argv[2]);
    return 0;
}


const char* cmd_to_str(operation cmd) {
    if(cmd == LIST) return "LIST";
    if(cmd == PULL) return "PULL";
    if(cmd == PUSH) return "PUSH";
    
    return "INVALID";
}


int parse_command(const char* buffer, command *full_command){
    
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

        token = strtok(NULL, "");        
        if(!token) return 1;

        strncpy(full_command->data, token, BUFFSIZ - 1);
        full_command->data[BUFFSIZ - 1] = '\0';

        return 0;
    }

    return 1;
}




int get_filenames(const char *path, char filenames[][BUFFSIZ], int max_files){
    printf("PATH %s\n\n", path);
    char buf[BUFFSIZ];
    int dir_fd = open(path, O_RDONLY | O_DIRECTORY);
    if(dir_fd == -1) {
        perror("open dir");
        return -1;
    }

    int nread;
    int file_count = 0;
    while((nread = getdents64(dir_fd, buf, BUFFSIZ * 4)) > 0){
        int bpos = 0;
        while(bpos < nread){
            struct linux_dirent64 *dir = (struct linux_dirent64 *)(buf + bpos);
            
            // Skip .. and .
            if(strcmp(dir->d_name, ".") && strcmp(dir->d_name, "..")){
                strncpy(filenames[file_count], dir->d_name, BUFFSIZ - 1);
                filenames[file_count][BUFFSIZ - 1] = '\0';
                file_count++;                
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






int write_all(int fd, const void *buff, size_t size) {
    int sent, n;
    for(sent = 0; sent < size; sent+=n) {
        if ((n = write(fd, buff+sent, size-sent)) == -1) return -1;
    }

    return sent;
}



int exec_command(const command cmd, int newsock){
    switch(cmd.op){
        case LIST: {
            char src_files[100][BUFFSIZ];
            int src_files_count = get_filenames(cmd.path, src_files, BUFFSIZ * 4);\
            if(src_files_count == -1) return -1;

            for(int i = 0; i < src_files_count; i++){
                char output_buff[BUFFSIZ];
                int len = snprintf(output_buff, sizeof(output_buff), "%s\n", src_files[i]);                
                write(newsock, output_buff, len);
            }
            write(newsock, ".\n", 2);

            break;
        }
        case PULL: {
            int fd_file_read = 0;
            if((fd_file_read = open(cmd.path, O_RDONLY)) == -1){
                
                char error_msg[] = "-1\n";
                write_all(newsock, error_msg, strlen(error_msg));
                
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
                    break;
                }
            }

            close(fd_file_read);
            break;
        }
        case PUSH: {
            if(!cmd.chunk_size) break; // final chunk, nothing to write

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
            break;
        }
    default: break;
    }

    return 0;
}

void handle_sigint(int sig) {
    client_active = 0;
}


// lsof -i :port
int main(int argc, char* argv[]){
    int port = 0;   
    if(arg_check(argc, (const char**)argv, &port)){
        perror("USAGE");
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
    while(client_active){
        clientlen = sizeof(struct sockaddr_in);

        int newsock = 0;
        if((newsock = accept(sock, clientptr, &clientlen)) < 0) 
            perror("accept ");

        printf("Accepted connection \n") ;

        char buffer[BUFFSIZ];
        memset(buffer, 0, sizeof(buffer));

        ssize_t n = read(newsock, buffer, sizeof(buffer) - 1);
        
        if(n <= 0) {
            perror("read");
            close(newsock);
            continue;
        }
        
        buffer[strcspn(buffer, "\n")] = '\0';

        command cmd;
        if(parse_command(buffer, &cmd)){
            perror("Invalid Command\n");
        }
        

        // printf("Message Received: %s\n", buffer);
        // printf("Parsed Command: %s\n", cmd_to_str(cmd.op));
        // printf("Directory Path: %s\n", cmd.path);
        // if(cmd.op == PUSH) {
        //     printf("chunk size: %d\n", cmd.chunk_size);
        //     printf("Data: %s\n", cmd.data);
        // }

        if(exec_command(cmd, newsock) == -1){
            perror("exec_command");
        };


        
        close(newsock);
    }

    close(sock);
    printf("Exiting\n");

    return 0;
}