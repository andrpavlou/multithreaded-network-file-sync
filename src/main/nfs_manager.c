#include <fcntl.h>
#include <signal.h> 
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "add_cmd.h"
#include "cancel_cmd.h"
#include "logging_defs.h"
#include "manager_worker_pool.h"

volatile sig_atomic_t manager_active = 1;


static void handle_sigint(int sig)
{
    manager_active = 0;
}



// bin/nfs_manager -l logs/manager.log -c config.txt -n 5 -p 2525 -b 10
int main(int argc, char *argv[])
{
    char *logfile       = NULL;
    char *config_file   = NULL;
    
    int port            = -1;
    int worker_limit    = -1;
    int buffer_size     = -1;

    int check_args_status = check_args_manager(argc, argv, &logfile, &config_file, &worker_limit, &port, &buffer_size);
    if (check_args_status == 1) {
        perror("USAGE: ./nfs_manager -l <logfile> -c <config_file> -n <worker_limit> -p <port> -b <buffer_size>");
        return 1;
    }

    if (check_args_status == -1) {
        perror("ERROR: -b Must be > 0");
        return 1;
    }

    int fd_log;
    if ((fd_log = open(logfile, O_WRONLY | O_TRUNC | O_CREAT, 0664)) < 0) {
        #ifdef DEBUG
        perror("open logfile");
        #endif
        return 1;
    }

    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; 
    sigaction(SIGINT, &sa, NULL);


    // Config file parsing
    int fd_config;
    if ((fd_config = open(config_file, O_RDONLY)) < 0) {
        #ifdef DEBUG
        perror("Config file open problem");
        #endif
        return 1;
    }

    int total_config_pairs = line_counter_of_file(fd_config, BUFFSIZ);
    lseek(fd_config, 0, SEEK_SET); // Reset the pointer at the start of the file
    config_pairs *conf_pairs = malloc(sizeof(config_pairs) * total_config_pairs);
    create_cf_pairs(fd_config, conf_pairs);
    
    
    sync_task_ts queue_tasks;
    if (init_sync_task_ts(&queue_tasks, buffer_size)) {
        #ifdef DEBUG
        perror("init sync task");
        #endif
        
        free(conf_pairs);
        return 1;
    }

    sync_info_mem_store *sync_info_head = NULL;

    struct sockaddr_in server, client;
    struct sockaddr *serverptr = (struct sockaddr *) &server;
    struct sockaddr *clientptr = (struct sockaddr *) &client;

    int socket_manager = 0;
    if ((socket_manager = socket(AF_INET , SOCK_STREAM , 0)) < 0) {
        #ifdef DEBUG
        perror("socket");
        #endif
    }
    
    int optval = 1;
    setsockopt(socket_manager, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    server.sin_family       = AF_INET; /* Internet domain */
    server.sin_addr.s_addr  = htonl(INADDR_ANY);
    server.sin_port         = htons(port); /* The given port */
    
    /* Bind socket to address */
    if (bind(socket_manager, serverptr, sizeof(server)) < 0) {
        #ifdef DEBUG
        perror("bind");
        #endif
        
        return 1;
    }

    if (listen(socket_manager, 5) < 0) {
        #ifdef DEBUG
        perror("listen");
        #endif
        
        return 1;
    }
    #ifdef DEBUG
    printf("Listening for connections to port % d \n", port);
    #endif
    
    socklen_t clientlen     = sizeof(struct sockaddr_in);
    int socket_console_read = 0;
    if ((socket_console_read = accept(socket_manager, clientptr, &clientlen)) < 0) {
        #ifdef DEBUG
        perror("accept ");
        #endif

        return 1;
    }


    bool conf_pairs_sync = FALSE;

    // Thread pool, ready to execute tasks
    pthread_t *worker_th = malloc(worker_limit * sizeof(pthread_t));
    for (int i = 0; i < worker_limit; i++) {
        thread_args *arglist = malloc(sizeof(thread_args));

        arglist->queue              = &queue_tasks;
        arglist->log_fd             = fd_log;
        arglist->write_console_sock = socket_console_read;

        pthread_create(&worker_th[i], NULL, exec_task_manager_th, arglist);
    }


    if (conf_pairs_sync == FALSE) {
        enqueue_config_pairs(total_config_pairs, &sync_info_head, &queue_tasks, conf_pairs, fd_log, -1);
        conf_pairs_sync = TRUE;
    }
    
    #ifdef DEBUG
    printf("Accepted connection \n") ;
    #endif
    
    while (manager_active) {
        char read_buffer[BUFFSIZ];
        memset(read_buffer, 0, sizeof(read_buffer));
        
        ssize_t n_read;
        if ((n_read = read(socket_console_read, read_buffer, BUFFSIZ - 1)) <= 0) {
            #ifdef DEBUG
            perror("Read from console");
            #endif
            return 1;
        }
        read_buffer[n_read] = '\0';

        manager_command curr_cmd;
        char *source_full_path = NULL;
        char *target_full_path = NULL;
        if (parse_console_command(read_buffer, &curr_cmd, &source_full_path, &target_full_path)) {
            #ifdef DEBUG
            fprintf(stderr, "error: parse_command [%s]\n", read_buffer);
            #endif

            write(socket_console_read, "END\n", 4);
            continue;
        }

        if (curr_cmd.op == ADD) {
            int status = enqueue_add_cmd(curr_cmd, &queue_tasks, &sync_info_head, source_full_path, target_full_path, fd_log, socket_console_read);
            if (status) {
                #ifdef DEBUG
                perror("enqueue add");
                #endif
            }

            free(source_full_path);
            free(target_full_path);

            write(socket_console_read, "END\n", 4);
        }

        if (curr_cmd.op == CANCEL) {
            int status = enqueue_cancel_cmd(curr_cmd, &queue_tasks, &sync_info_head, source_full_path);
            if (status) {
                LOG_CANCEL_NOT_MONITORED(curr_cmd, socket_console_read);
                write(socket_console_read, "END\n", 4);
            }

            // Thread will send "END\n"
            free(source_full_path);
        }

        if(curr_cmd.op == SHUTDOWN) manager_active = 0;
    }

    LOG_MANAGER_SHUTDOWN_SEQUENCE(socket_console_read); 




    // *********** QUEUE THE POISON PILLS *********** //
    for (int i = 0; i < worker_limit; i++) {
        sync_task *shutdown_task = calloc(1, sizeof(sync_task));
        shutdown_task->manager_cmd.op = SHUTDOWN;

        enqueue_task(&queue_tasks, shutdown_task);
    }

    for (int i = 0; i < worker_limit; i++) {
        pthread_join(worker_th[i], NULL);
    }

    LOG_MANAGER_SHUTDOWN_FINAL(socket_console_read);

    close(fd_log);
    free(conf_pairs);
    free(worker_th);
    close(socket_manager);
    close(socket_console_read);
    free_queue_task(&queue_tasks);
    free_all_sync_info(&sync_info_head);

    
    return 0;
}