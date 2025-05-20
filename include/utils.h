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


int line_counter_of_file(int fd, int max_len);
int create_cf_pairs(int fd_config, config_pairs *conf_pairs);
int read_next_line_from_file(int fd, char *line, int max_len);

#endif