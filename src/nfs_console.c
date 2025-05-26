#include "nfs_console.h"
#include "nfs_manager.h"

volatile sig_atomic_t console_active = 1;
void handle_sigint(int sig){
    console_active = 0;
}



int parse_console_command(const char* buffer, client_command *full_command){
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



// bin/nfs_console -l logs/console.log -h localhost -p 2525
int main(int argc, char* argv[]){
    int host_port;
    char *logfile, *host_ip;

    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; 
    sigaction(SIGINT, &sa, NULL);
    

    if(check_args_console(argc, argv, &logfile, &host_ip, &host_port)){
        perror("Usage: ./nfs_console -l <console-logfile> -h <host_IP> -p <host_port>");
        return 1;
    }
    
    int socket_host_manager;
    if(establish_connection(&socket_host_manager, host_ip, host_port)){
        perror("establish con");
        exit(1);
    }


    printf("Give input: ");
    while(console_active){
        char console_buffer[BUFFSIZ];
        printf("Give input: ");
        ssize_t read_b = read(0, console_buffer, BUFFSIZ - 1);

        ssize_t write_b = write_all(socket_host_manager, console_buffer, read_b);
        if(write_b == -1){
            perror("write");
        }

        printf("Bytes written: %ld\n", write_b);
    }


    close(socket_host_manager);
    return 0;
}