#ifndef NFS_MANAGER_H
#define NFS_MANAGER_H


#include "nfs_client.h"
#include <sys/types.h>
#include <stddef.h>

#define FILE_NF_LEN -1
#define BUFFSIZ     256
#define MAX_FILES   100


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

} config_pairs;



#endif