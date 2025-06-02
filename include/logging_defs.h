#ifndef LOGGING_DEFS_H
#define LOGGING_DEFS_H



// ADD QUEUE LOGS

#define ADDED_FILE_LOG(new_task, fd_log) do{                                                    \
    char log_buffer[BUFSIZ * 8];                                                                \
    snprintf(log_buffer, sizeof(log_buffer), "[%s] Added file: %s/%s@%s:%d -> %s/%s@%s:%d\n",   \
        get_current_time_str(),                                                                 \
        (new_task)->manager_cmd.source_dir,                                                     \
        (new_task)->filename,                                                                   \
        (new_task)->manager_cmd.source_ip,                                                      \
        (new_task)->manager_cmd.source_port,                                                    \
        (new_task)->manager_cmd.target_dir,                                                     \
        (new_task)->filename,                                                                   \
        (new_task)->manager_cmd.target_ip,                                                      \
        (new_task)->manager_cmd.target_port);                                                   \
    write(1, log_buffer, strlen(log_buffer));                                                   \
    write((fd_log), log_buffer, strlen(log_buffer));                                            \
} while(0)


#define ALREADY_IN_QUEUE_LOG(curr_cmd, filename, fd_log) do{                                    \
    char log_buffer[BUFSIZ * 4];                                                                \
    snprintf(log_buffer, sizeof(log_buffer), "[%s] Already in queue: %s/%s@%s:%d\n",            \
        get_current_time_str(),                                                                 \
        (curr_cmd).source_dir,                                                                  \
        (filename),                                                                             \
        (curr_cmd).source_ip,                                                                   \
        (curr_cmd).source_port);                                                                \
    write(1, log_buffer, strlen(log_buffer));                                                   \
    write((fd_log), log_buffer, strlen(log_buffer));                                            \
} while(0)




// PULL / PUSH LOGS

#define LOG_PULL_SUCCESS(curr_task, total_read_pull) do{ \
    char log_buffer[BUFSIZ * 4]; \
    snprintf(log_buffer, sizeof(log_buffer), \
        "[%s] [%s@%s:%d] [%s@%s:%d] [%ld] [%s] [%s] [%zd %s]\n", \
        get_current_time_str(), \
        (curr_task)->manager_cmd.source_dir, (curr_task)->manager_cmd.source_ip, (curr_task)->manager_cmd.source_port, \
        (curr_task)->manager_cmd.target_dir, (curr_task)->manager_cmd.target_ip, (curr_task)->manager_cmd.target_port, \
        pthread_self(), \
        "PULL", \
        "SUCCESS", \
        (total_read_pull), \
        "bytes pulled"); \
    write(1, log_buffer, strlen(log_buffer)); \
} while(0)



#define LOG_PUSH_SUCCESS(curr_task, total_write_push) do{ \
    char log_buffer[BUFSIZ * 4]; \
    snprintf(log_buffer, sizeof(log_buffer), \
        "[%s] [%s@%s:%d] [%s@%s:%d] [%ld] [%s] [%s] [%zd %s]\n", \
        get_current_time_str(), \
        (curr_task)->manager_cmd.source_dir, (curr_task)->manager_cmd.source_ip, (curr_task)->manager_cmd.source_port, \
        (curr_task)->manager_cmd.target_dir, (curr_task)->manager_cmd.target_ip, (curr_task)->manager_cmd.target_port, \
        pthread_self(), \
        "PUSH", \
        "SUCCESS", \
        (total_write_push), \
        "bytes pushed"); \
    write(1, log_buffer, strlen(log_buffer)); \
} while(0)



#endif