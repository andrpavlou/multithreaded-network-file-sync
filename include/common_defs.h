#ifndef COMMON_DEFS_H
#define COMMON_DEFS_H


#define BUFFSIZ 1024
#define MAX_FILES 100




#define MIN(size1, size2) ((size1) < (size2) ? (size1) : (size2))



typedef struct {
    char source_full_path[BUFFSIZ];
    char target_full_path[BUFFSIZ];
    

    char source_dir_path[BUFFSIZ];
    char source_ip[BUFFSIZ / 4];
    int source_port;

    char target_dir_path[BUFFSIZ];
    char target_ip[BUFFSIZ / 4];
    int target_port;

} config_pairs;


typedef enum { 
    FALSE, 
    TRUE
} bool;


#endif