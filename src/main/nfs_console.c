#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>


#include "logging_defs.h"
#include "socket_utils.h"
#include "read_from_manager.h"


/*
  socket()
    |
  connect() 
    |
  write()
    |
  read()
*/

volatile sig_atomic_t console_active = 1;

static void handle_sigint(int sig)
{
    console_active = 0;
}


// bin/nfs_console -l logs/console.log -h localhost -p 2525
int main(int argc, char* argv[])
{
    int host_port;
    char *logfile, *host_ip;

    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; 
    sigaction(SIGINT, &sa, NULL);
    
    
    if (check_args_console(argc, argv, &logfile, &host_ip, &host_port)) {
        perror("Usage: ./nfs_console -l <console-logfile> -h <host_IP> -p <host_port>");
        return 1;
    }
    
    int fd_log;
    if ((fd_log = open(logfile, O_CREAT | O_WRONLY | O_TRUNC, 0664)) == -1) {
        #ifdef DEBUG
        perror("open");
        #endif
        return 1;
    }

    int socket_host_manager;
    if (establish_connection(&socket_host_manager, host_ip, host_port)) {
        #ifdef DEBUG
        perror("establish con");
        #endif
        
        close(fd_log);
        return 1;
    }

    
    while (console_active) {
        write(1, "> ", 2);
        char console_buffer[BUFFSIZ];
        
        ssize_t read_b = read(0, console_buffer, BUFFSIZ - 1);
        if (read_b < 0) {
            #ifdef DEBUG
            perror("read");
            #endif
            continue;
        }
        
        console_buffer[strcspn(console_buffer, "\n")] = '\0';
        LOG_CONSOLE_COMMANDS(read_b, console_buffer, fd_log);

        ssize_t write_b = write_all(socket_host_manager, console_buffer, read_b);
        if (write_b == -1) {
            #ifdef DEBUG
            perror("write");
            #endif
        }

        if (!strncasecmp(console_buffer, "shutdown", 9)) {
            console_active = 0;
            continue;
        } 
        read_from_manager(socket_host_manager);
    }
    
    read_from_manager(socket_host_manager);
    
    close(fd_log);
    close(socket_host_manager);
    
    return 0;
}