#ifndef MANAGER_DEF_H
#define MANAGER_DEF_H

#include "common_defs.h"

typedef enum{
    ADD,
    CANCEL,
    SHUTDOWN,
} manager_operation;

typedef struct {
    manager_operation op;
    char full_path[BUFFSIZ];
    char source_ip[BUFFSIZ];      // list + pull: dir for push
    char target_ip[BUFFSIZ];      // push

    char source_dir[BUFFSIZ];
    char target_dir[BUFFSIZ];

    int source_port;
    int target_port;


    char cancel_dir[BUFFSIZ];
} manager_command;



#endif