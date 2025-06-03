#include <sys/select.h>
#include "common.h"
#include "utils.h"


volatile sig_atomic_t console_active = 1;
void handle_sigint(int sig){
    console_active = 0;
}


ssize_t read_line_manager(int sock, char *buffer, size_t max_len) {
    size_t total_read = 0, read_b;
    char c;
    do{
        read_b = read(sock, &c, 1);
        
        if(read_b == 0) break;
        if(read_b < 0)  return -1;

        buffer[total_read++] = c;
    } while (total_read < max_len - 1 && c != '\n');
      
    buffer[total_read] = '\0';
    return total_read;
}


int read_from_manager(int sock_fd) {
    char buffer[BUFFSIZ];

    while(1){
        ssize_t read_b;
        if((read_b = read_line_manager(sock_fd, buffer, sizeof(buffer))) == 0) break;
        if(read_b < 0) return -1;

        if(!strncmp(buffer, "END\n", 4)) break;

        write(1, buffer, read_b); 
    }

    return 0;
}



// TODO: what todo with invalid commands

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

    
    while(console_active){
        write(1, "> ", 2);
        char console_buffer[BUFFSIZ];
        
        ssize_t read_b = read(0, console_buffer, BUFFSIZ - 1);
        if(read_b < 0){
            perror("read");
            continue;
        }
        console_buffer[strcspn(console_buffer, "\n")] = '\0';


        ssize_t write_b = write_all(socket_host_manager, console_buffer, read_b);
        if(write_b == -1){
            perror("write");
        }

        if(!strncasecmp(console_buffer, "shutdown", 9)) console_active = 0;

        read_from_manager(socket_host_manager);
    }

    printf("exiting\n");
    close(socket_host_manager);
    return 0;
}