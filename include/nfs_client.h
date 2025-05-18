#ifndef NFS_CLIENT_H
#define NFS_CLIENT_H

#define BUFSIZ 256


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



typedef enum{
    LIST,
    PULL,
    PUSH, 
    INVALID
} command;


#endif