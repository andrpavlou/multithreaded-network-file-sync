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

struct linux_dirent64 {
    ino64_t        d_ino;
    off64_t        d_off;
    unsigned short d_reclen;
    unsigned char  d_type;
    char           d_name[];
};

#define CHECK_READ(read_b, total_read, buffer)  \
    do {                                        \
        if((read_b) == 0){                      \
            buffer[total_read] = '\0';          \
            return total_read;                  \
        }                                       \
        if((read_b) < 0) return -1;             \
    } while(0) 


#endif