#ifndef LOGGING_DEFS_H
#define LOGGING_DEFS_H

#include "utils.h"


////////////////// ADD QUEUE LOGS //////////////////

#define ADDED_FILE_LOG(new_task, fd_log, write_sock) do{ \
    char log_buffer[BUFSIZ * 8]; \
    snprintf(log_buffer, sizeof(log_buffer), "[%s] Added file: %s/%s@%s:%d -> %s/%s@%s:%d\n", \
        get_current_time_str(), \
        (new_task)->manager_cmd.source_dir, \
        (new_task)->filename, \
        (new_task)->manager_cmd.source_ip, \
        (new_task)->manager_cmd.source_port, \
        (new_task)->manager_cmd.target_dir, \
        (new_task)->filename, \
        (new_task)->manager_cmd.target_ip, \
        (new_task)->manager_cmd.target_port); \
    write(1, log_buffer, strlen(log_buffer)); \
    write((fd_log), log_buffer, strlen(log_buffer)); \
    if((write_sock) != -1) write((write_sock), log_buffer, strlen(log_buffer)); \
} while(0)


#define ALREADY_IN_QUEUE_LOG(curr_cmd, filename, write_sock) do{ \
    char log_buffer[BUFSIZ * 4];  \
    snprintf(log_buffer, sizeof(log_buffer), "[%s] Already in queue: %s/%s@%s:%d\n",  \
        get_current_time_str(),  \
        (curr_cmd).source_dir,  \
        (filename),  \
        (curr_cmd).source_ip,  \
        (curr_cmd).source_port);  \
    write(1, log_buffer, strlen(log_buffer)); \
    write((write_sock), log_buffer, strlen(log_buffer)); \
} while(0)




////////////////// PULL/PUSH LOGS //////////////////

#define LOG_PULL_SUCCESS(curr_task, total_read_pull, log_fd) do{ \
    char log_buffer[BUFSIZ * 4]; \
    snprintf(log_buffer, sizeof(log_buffer), \
        "[%s] [%s/%s@%s:%d] [%s/%s@%s:%d] [%ld] [%s] [%s] [%zd %s]\n", \
        get_current_time_str(), \
        (curr_task)->manager_cmd.source_dir, (curr_task)->filename, (curr_task)->manager_cmd.source_ip, (curr_task)->manager_cmd.source_port, \
        (curr_task)->manager_cmd.target_dir, (curr_task)->filename, (curr_task)->manager_cmd.target_ip, (curr_task)->manager_cmd.target_port, \
        pthread_self(), \
        "PULL", \
        "SUCCESS", \
        (total_read_pull), \
        "bytes pulled"); \
    write((log_fd), log_buffer, strlen(log_buffer)); \
} while(0)


#define LOG_PULL_ERROR(curr_task, err_buff, log_fd) do{ \
    char log_buffer[BUFSIZ * 4];    \
    int log_len = snprintf(log_buffer, sizeof(log_buffer), \
                    "[%s] [%s/%s@%s:%d] [%s/%s@%s:%d] [%ld] [%s] [%s] [File: %s %s]\n", \
                    get_current_time_str(), \
                    (curr_task)->manager_cmd.source_dir, (curr_task)->filename, (curr_task)->manager_cmd.source_ip, (curr_task)->manager_cmd.source_port, \
                    (curr_task)->manager_cmd.target_dir, (curr_task)->filename, (curr_task)->manager_cmd.target_ip, (curr_task)->manager_cmd.target_port, \
                    pthread_self(), \
                    "PULL", \
                    "ERROR", \
                    (curr_task)->filename, \
                    (err_buff)); \
    write(fd_log, log_buffer, log_len); \
} while(0)



#define LOG_PUSH_SUCCESS(curr_task, total_write_push, log_fd) do{ \
    char log_buffer[BUFSIZ * 4]; \
    snprintf(log_buffer, sizeof(log_buffer), \
        "[%s] [%s/%s@%s:%d] [%s/%s@%s:%d] [%ld] [%s] [%s] [%zd %s]\n", \
        get_current_time_str(), \
        (curr_task)->manager_cmd.source_dir, (curr_task)->filename, (curr_task)->manager_cmd.source_ip, (curr_task)->manager_cmd.source_port, \
        (curr_task)->manager_cmd.target_dir, (curr_task)->filename, (curr_task)->manager_cmd.target_ip, (curr_task)->manager_cmd.target_port, \
        pthread_self(), \
        "PUSH", \
        "SUCCESS", \
        (total_write_push), \
        "bytes pushed"); \
    write(log_fd, log_buffer, strlen(log_buffer)); \
} while(0)


////////////////// CANCEL LOGS //////////////////

#define LOG_CANCEL_SUCCESS(curr_task, log_fd, sock_write) do{ \
    char log_buffer[BUFSIZ * 4]; \
    snprintf(log_buffer, sizeof(log_buffer), \
        "[%s] Synchronization stopped for: /%s@%s:%d\n", \
        get_current_time_str(), \
        (curr_task)->manager_cmd.cancel_dir , (curr_task)->manager_cmd.source_ip, (curr_task)->manager_cmd.source_port); \
    write((sock_write), log_buffer, strlen(log_buffer)); \
    write((log_fd), log_buffer, strlen(log_buffer)); \
    write(1, log_buffer, strlen(log_buffer)); \
} while(0)




/*
*   LOG_CANCEL_NOT_MONITORED VS LOG_CANCEL_NOT_MONITORED_THREAD
*   ------------------------------------------------------------
*   LOG_CANCEL_NOT_MONITORED: The task did not even get to be queued, because it does not exist
*   in the our known hosts that were previously used, so we could not identify the ip:port.
*   
*
*   LOG_CANCEL_NOT_MONITORED_THREAD: The task managed to be queued, so the worker threads,
*   successfully dequeued this task, but all the syncing tasks were finished before this 
*   canceling task was queued -> so there is nothing to cancel. In this way we know the hosts
*   and log fully log them.
*   
*   (note: by the time a canceling task is given, it is given a priority, to be queued on top of the queue).
*
*/

#define LOG_CANCEL_NOT_MONITORED(curr_cmd, write_sock) do{ \
    char log_buffer[BUFSIZ * 4]; \
    snprintf(log_buffer, sizeof(log_buffer), \
        "[%s] Directory not being synchronized: %s/\n", \
        get_current_time_str(), \
        (curr_cmd).cancel_dir); \
    write((write_sock), log_buffer, strlen(log_buffer)); \
    write(1, log_buffer, strlen(log_buffer)); \
} while(0)



#define LOG_CANCEL_NOT_MONITORED_THREAD(curr_cmd, log_fd) do{ \
    char log_buffer[BUFSIZ * 4]; \
    snprintf(log_buffer, sizeof(log_buffer), \
        "[%s] Directory not being synchronized: /%s/%s@%s:%d\n", \
        get_current_time_str(), \
        (curr_task)->manager_cmd.cancel_dir, (curr_task)->filename, (curr_task)->manager_cmd.source_ip, (curr_task)->manager_cmd.source_port); \
    write(log_fd, log_buffer, strlen(log_buffer)); \
    write(1, log_buffer, strlen(log_buffer)); \
} while(0)


////////////////// CONSOLE LOGS //////////////////

#define LOG_CONSOLE_COMMANDS(read_b, console_buffer, fd_log) do{ \
    char console_log[BUFFSIZ * 2]; \
    if((read_b)){ \
        int console_log_len = snprintf(console_log, sizeof(console_log), "[%s] Command %s\n", get_current_time_str(), (console_buffer)); \
        write((fd_log), (console_log), console_log_len); \
    } \
} while(0)



////////////////// MANAGER EXIT //////////////////

#define LOG_MANAGER_SHUTDOWN_SEQUENCE(write_socket)do{ \
    char log_buf[256]; \
    snprintf(log_buf, sizeof(log_buf), "[%s] Shutting down manager...\n", get_current_time_str()); \
    write(write_socket, log_buf, strlen(log_buf)); \
    write(1, log_buf, strlen(log_buf)); \
    \
    snprintf(log_buf, sizeof(log_buf), "[%s] Waiting for all active workers to finish.\n", get_current_time_str()); \
    write(write_socket, log_buf, strlen(log_buf)); \
    write(1, log_buf, strlen(log_buf)); \
    \
    snprintf(log_buf, sizeof(log_buf), "[%s] Processing remaining queued tasks.\n", get_current_time_str()); \
    write(write_socket, log_buf, strlen(log_buf)); \
    write(1, log_buf, strlen(log_buf)); \
} while(0)


#define LOG_MANAGER_SHUTDOWN_FINAL(write_socket)do{ \
    char log_buf[256]; \
    snprintf(log_buf, sizeof(log_buf), "[%s] Manager shutdown complete.\n", get_current_time_str()); \
    write(write_socket, log_buf, strlen(log_buf)); \
    write(1, log_buf, strlen(log_buf)); \
    \
    write(write_socket, "END\n", 4); \
} while(0)


////////////////// LIST COMMAND ERROR //////////////////

#define LOG_LIST_ERROR(curr_cmd, source_full_path, list_reply_buff, write_sock, fd_log) do { \
    char log_buffer[BUFSIZ * 4]; \
    snprintf(log_buffer, sizeof(log_buffer), \
        "[%s] [%s] [LIST] [ERROR] [Directory: %s - %s]\n", \
        get_current_time_str(), \
        (source_full_path), \
        (curr_cmd).source_dir, \
        (list_reply_buff)); \
    write((write_sock), log_buffer, strlen(log_buffer)); \
    write((fd_log), log_buffer, strlen(log_buffer)); \
    write(1, log_buffer, strlen(log_buffer)); \
} while(0)



#endif