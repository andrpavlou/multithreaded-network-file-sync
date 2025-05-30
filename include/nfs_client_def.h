#ifndef NFS_CLIENT_DEF_H
#define NFS_CLIENT_DEF_H


typedef enum{
    LIST,
    PULL,
    PUSH, 
    INVALID
} client_operation;



typedef struct {
    client_operation op;
    char path[BUFFSIZ];      // list + pull: dir for push
    char data[BUFFSIZ];      // push
    int chunk_size;         // push
} client_command;


#endif