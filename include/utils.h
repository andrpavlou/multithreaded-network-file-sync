#ifndef UTILS_H
#define UTILS_H

#include <sys/types.h>

#include "common_defs.h"
#include "nfs_manager_def.h"

int line_counter_of_file(int fd, int max_len);
int create_cf_pairs(int fd_config, config_pairs *conf_pairs);
int read_next_line_from_fd(int fd, char *line, int max_len);

int check_args_manager(int argc, char *argv[], char** logfile, char** config_file, int *worker_limit, int *port, int *buffer_size);
int check_args_console(int argc, char* argv[], char **logfile, char **host_ip, int *port);
int check_args_client(int argc, const char** argv, int *port);

int parse_console_command(const char* buffer, manager_command *full_command, char **source_full_path, char **target_full_path);
int parse_pull_read(const char *rec_buff, ssize_t *rec_file_len, char **file_content);
int parse_cmd_path(const char *full_path, char *dir_path, char *ip, int *port);

char* get_current_time_str(void);

#endif