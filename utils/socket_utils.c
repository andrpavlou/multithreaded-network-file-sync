#include "socket_utils.h"

#include <stdio.h>          
#include <stdlib.h>         
#include <string.h>         
#include <unistd.h>         
#include <sys/types.h>      
#include <sys/socket.h>     
#include <netinet/in.h>     
#include <arpa/inet.h>      
#include <netdb.h>          


int establish_connection(int *sock, const char *ip, const int port){
    struct sockaddr_in servadd; /* The address of server */
    struct hostent *hp;         /* to resolve server ip */

    if((*sock = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        #ifdef DEBUG
        perror("socket");
        #endif
        return 1;
    }
    
    if((hp = gethostbyname(ip)) == NULL){
        #ifdef DEBUG
        perror("gethostbyname"); 
        #endif
        close(*sock);
        return 1;
    }

    memcpy(&servadd.sin_addr, hp->h_addr, hp->h_length);
    servadd.sin_port = htons(port); /* set port number */
    servadd.sin_family = AF_INET;   /* set socket type */

    if(connect(*sock, (struct sockaddr*) &servadd, sizeof(servadd)) != 0){
        #ifdef DEBUG
        perror("connect");
        #endif
        close(*sock);
        return 1;
    }
    return 0;
}



ssize_t write_all(int fd, const void *buf, size_t size) {
    ssize_t total_written = 0;
    while(total_written < size){
        ssize_t n = write(fd, (char*)buf + total_written, size - total_written);
        
        if(n < 0)   return -1;
        if(n == 0)  break;
        
        total_written += n;
    }

    return total_written;
}


ssize_t read_all(int fd, void *buf, size_t size){
    ssize_t total_read_b = 0;

    while(total_read_b < size){
        ssize_t n = read(fd, (char*) buf + total_read_b, size - total_read_b);
        
        if(n <= 0) return n;
        if(n == 0) return -1;

        total_read_b += n;
    }

    return total_read_b;
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




int read_list_response(int sock, char **list_reply_buff){
    size_t total_read_list      = 0;
    size_t list_buff_capacity   = 0;

    do{
        // Allocate more memory for buffer if there is not enough space
        if(list_buff_capacity - total_read_list < BUFFSIZ){
            list_buff_capacity += BUFFSIZ;
            *list_reply_buff = realloc(*list_reply_buff, list_buff_capacity);

            if(*list_reply_buff == NULL){
                #ifdef DEBUG
                perror("Realloc");
                #endif
                return 1;
            }
            (*list_reply_buff)[list_buff_capacity - 1] = '\0';
        }
        ssize_t n_read = read(sock, *list_reply_buff + total_read_list, BUFFSIZ);
        
        if(n_read <= 0){
            *((*list_reply_buff) + total_read_list - 1) = '\0';
            return 1;
        }

        total_read_list += n_read;
        (*list_reply_buff)[total_read_list] = '\0';
    } while(strstr((*list_reply_buff), "\n.") == NULL); // Keep reading until we receive '.\n'

    return 0;
}