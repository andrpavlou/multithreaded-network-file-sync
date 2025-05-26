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

struct linux_dirent64 {
    ino64_t        d_ino;
    off64_t        d_off;
    unsigned short d_reclen;
    unsigned char  d_type;
    char           d_name[];
};

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
    char source[BUFFSIZ];      // list + pull: dir for push
    char target[BUFFSIZ];      // push
} manager_command;


#endif