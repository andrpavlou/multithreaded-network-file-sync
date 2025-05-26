#ifndef UTILS_H
#define UTILS_H

#include "nfs_manager.h"
#include <string.h>
#include <unistd.h>
#include <stdio.h>     
#include <unistd.h>    
#include <sys/types.h> 
#include <sys/stat.h>  
#include <stdio.h>

typedef struct {
    char source_full_path[BUFFSIZ];
    char target_full_path[BUFFSIZ];
    
    pid_t worker_pid;

    char source_dir_path[BUFFSIZ];
    char source_ip[BUFFSIZ / 4];
    int source_port;

    char target_dir_path[BUFFSIZ];
    char target_ip[BUFFSIZ / 4];
    int target_port;

} config_pairs; // TODO: make sure how the path will be given



int line_counter_of_file(int fd, int max_len);
int create_cf_pairs(int fd_config, config_pairs *conf_pairs);
int read_next_line_from_file(int fd, char *line, int max_len);
int check_args_manager(int argc, char *argv[], char** logfile, char** config_file, int *worker_limit, int *port, int *buffer_size);
int check_args_console(int argc, char* argv[], char **logfile, char **host_ip, int *port);

//////////// TODO: socket related must add these to other modules
int establish_connection(int *sock, const char *ip, const int port);
ssize_t write_all(int fd, const void *buff, size_t size);
ssize_t read_all(int fd, void *buf, size_t size);
int receive_ack(int sock);
long get_file_size_of_host(int sock);
int get_files_from_list_response(char *list_reply_buff, char *file_buff[MAX_FILES]);
int read_list_response(int sock, char **list_reply_buff);
int parse_pull_read(const char *rec_buff, ssize_t *rec_file_len, char **file_content);



#endif