#ifndef NFS_CLIENT_H
#define NFS_CLIENT_H

#define _GNU_SOURCE
#define BUFFSIZ 256
#define MAX_FILES 100


#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <sys/wait.h> /* sockets */
#include <sys/types.h> /* sockets */
#include <sys/socket.h> /* sockets */
// #include <netinet/in .h>
#include <netdb.h> /* gethostbyaddr */
#include <unistd.h> /* fork */
#include <stdlib.h> /* exit */
#include <ctype.h> /* toupper */
#include <signal.h> /* signal */


#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <stdint.h>
#include <limits.h>
#include <time.h>
#include <sys/time.h>




typedef enum{
    LIST,
    PULL,
    PUSH, 
    INVALID
} client_operation;

typedef enum{
    ADD,
    CANCEL,
    SHUTDOWN,
} manager_operation;


typedef struct {
    client_operation op;
    char path[BUFFSIZ];      // list + pull: dir for push
    char data[BUFFSIZ];      // push
    int chunk_size;         // push
} client_command;


typedef struct {
    manager_operation op;
    char full_path[BUFFSIZ];
    char source_ip[BUFFSIZ];      // list + pull: dir for push
    char target_ip[BUFFSIZ];      // push

    char source_dir[BUFFSIZ];
    char target_dir[BUFFSIZ];

    int source_port;
    int target_port;


    char cancel_dir[BUFFSIZ]; // TODO: might just need to seperate cancel/add cmd 
} manager_command;


#endif