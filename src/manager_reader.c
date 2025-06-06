#include <unistd.h>   
#include <string.h>   
#include <stddef.h>

#include "manager_reader.h"
#include "common_defs.h"

static ssize_t read_line_manager(int socket, char *buffer, size_t max_len){
    size_t total_read = 0, read_b;
    char c;
    do{
        read_b = read(socket, &c, 1);
        
        if(read_b == 0) break;
        if(read_b < 0)  return -1;

        buffer[total_read++] = c;
    }while(total_read < max_len - 1 && c != '\n');
      
    buffer[total_read] = '\0';
    return total_read;
}


int read_from_manager(int sock_fd){
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
