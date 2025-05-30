#include "nfs_console.h"
#include "nfs_manager.h"

volatile sig_atomic_t console_active = 1;
void handle_sigint(int sig){
    console_active = 0;
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


    
    while(console_active){
        printf(">");
        fflush(stdout);
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

        printf("Bytes written: %ld\n", write_b);
    }

    printf("exiting\n");
    close(socket_host_manager);
    return 0;
}