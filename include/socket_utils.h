#ifndef SOCKET_UTILS_H
#define SOCKET_UTILS_H

#include <sys/types.h>

#include "common_defs.h"


int establish_connection(int *sock, const char *ip, const int port);
ssize_t write_all(int fd, const void *buff, size_t size);
ssize_t read_all(int fd, void *buff, size_t size);
long get_file_size_of_host(int sock);
int get_files_from_list_response(char *list_reply_buff, char *file_buff[MAX_FILES]);
int read_list_response(int sock, char **list_reply_buff);

#endif