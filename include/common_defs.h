#ifndef COMMON_DEFS_H
#define COMMON_DEFS_H

#define _GNU_SOURCE

#define BUFFSIZ 256
#define MAX_FILES 100



#define CLEANUP(sock_target_push, sock_source_read, curr_task) do { \
    close(sock_target_push); \
    close(sock_source_read); \
    free(curr_task); \
} while (0)

#define CLOSE_SOCKETS(sock_target_push, sock_source_read) do { \
    close(sock_target_push); \
    close(sock_source_read); \
} while (0)

#define MIN(size1, size2) ((size1) < (size2) ? (size1) : (size2))



#endif