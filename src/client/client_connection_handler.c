#define _GNU_SOURCE

#include <stdio.h>      
#include <stdlib.h>     
#include <string.h>     
#include <unistd.h>     
#include <fcntl.h>      
#include <errno.h>      
#include <dirent.h>     


#include "common_defs.h"
#include "client_connection_handler.h"
#include "socket_utils.h"

static ssize_t read_command_from_manager(int socket, char *buffer, ssize_t max_read)
{
    char current_char;
    ssize_t total_read = 0;

    while (total_read < max_read - 1) {
        ssize_t read_b = read(socket, &current_char, 1);
        CHECK_READ(read_b, total_read, buffer);

        buffer[total_read++] = current_char;
        
        if (current_char == '\r') {
            // Found \r so read one more byte to see if the pattern \r\n if found
            read_b = read(socket, &current_char, 1);
            CHECK_READ(read_b, total_read, buffer);

            if (current_char == '\n') {
                // Pattern found so just remove \r
                buffer[total_read - 1] = '\0';
                return total_read - 1;
            }

            // \r\n not found yet, just keep going
            buffer[total_read++] = current_char;
        }
    }
    
    buffer[total_read] = '\0';
    return total_read;
}

static int parse_manager_command(const char* buffer, client_command *full_command)
{
    memset(full_command->data, 0, BUFFSIZ);

    if (!strncmp(buffer, "LIST", 4)) {
        full_command->op = LIST;
        strncpy(full_command->path, &buffer[5], BUFFSIZ - 1);
        full_command->path[BUFFSIZ - 1] = '\0';
        
        return 0;
    }

    if (!strncmp(buffer, "PULL", 4)) {
        full_command->op = PULL;
        strncpy(full_command->path, &buffer[5], BUFFSIZ - 1);
        full_command->path[BUFFSIZ - 1] = '\0';
        
        return 0;
    } 

    if (!strncmp(buffer, "PUSH", 4)) {
        full_command->op = PUSH;

        char tmp_buffer[2 * BUFFSIZ];
        strncpy(tmp_buffer, buffer, sizeof(tmp_buffer) - 1);
        tmp_buffer[sizeof(tmp_buffer) - 1] = '\0';
        
        char *saveptr;
        char *token = strtok_r(tmp_buffer, " ", &saveptr);
        
        // thread safe instead of strtok
        token = strtok_r(NULL, " ", &saveptr);
        if (!token) return 1;

        strncpy(full_command->path, token, BUFFSIZ - 1);
        full_command->path[BUFFSIZ - 1] = '\0';
        
        token = strtok_r(NULL, " ", &saveptr);
        if (!token) return 1;

        full_command->chunk_size = atoi(token);
        if (full_command->chunk_size < -1 || full_command->chunk_size > BUFFSIZ) {
            #ifdef DEBUG
            perror("invalid chunk size sent");
            #endif
            return 1;
        } 


        /* 
         * Will not copy full_command->data right now it will be read directly
         * in the exec command as the protocol is \r\n, so first read the command 
         * get the batch size then expect it 
         */
        if (full_command->chunk_size == -1) {
            full_command->data[0] = '\0';
            return 0;
        }
        
        // full_command->data[strlen(token) + 1] = '\0';
        return 0;
    }
    return 1;
}



static int get_filenames(const char *path, char filenames[][BUFFSIZ]){
    char buf[BUFFSIZ];
    
    int dir_fd = open(path, O_RDONLY | O_DIRECTORY);
    if(dir_fd == -1) {
        #ifdef DEBUG
        perror("open dir");
        #endif
        return -1;
    }

    int nread       = 0;
    int file_count  = 0;
    while((nread = getdents64(dir_fd, buf, BUFFSIZ)) > 0){
        int bpos = 0;
        while(bpos < nread){
            struct linux_dirent64 *dir = (struct linux_dirent64 *)(buf + bpos);
            
            // Skip previous dir and pwd
            if(strcmp(dir->d_name, ".") && strcmp(dir->d_name, "..")){
                strncpy(filenames[file_count], dir->d_name, BUFFSIZ - 1);
                filenames[file_count][BUFFSIZ - 1] = '\0';

                file_count++;
                if(file_count >= MAX_FILES) return 1;
            }
            bpos += dir->d_reclen;
        }
    }

    if(nread == -1){
        #ifdef DEBUG
        perror("getdents64");
        #endif
        return -1;
    }

    close(dir_fd);
    return file_count;
}



static int exec_cmd_client(client_command cmd, int newsock)
{
    if (cmd.op == LIST) {
        char clean_path[BUFFSIZ];
        strncpy(clean_path, cmd.path, BUFFSIZ - 1);
        // '\0' if there is newline in the path to ignore it
        clean_path[BUFFSIZ - 1] = '\0';
        clean_path[strcspn(clean_path, "\n")] = '\0';
        
        char src_files[MAX_FILES][BUFFSIZ];
        int src_files_count = get_filenames(clean_path, src_files);

        if (src_files_count == -1) {
            char error_buf[BUFFSIZ];
            int len = snprintf(error_buf, sizeof(error_buf), "%s.\n", strerror(errno));
            
            write(newsock, error_buf, len);
            return -1;
        } 

        for (int i = 0; i < src_files_count; i++) {
            char output_buff[BUFFSIZ];
            int len = snprintf(output_buff, sizeof(output_buff), "%s\n", src_files[i]);                
            write(newsock, output_buff, len);
        }
        write(newsock, ".\n", 2);

        return 0;
    }

    if (cmd.op == PULL) {
        int fd_file_read = 0;
        if ((fd_file_read = open(cmd.path, O_RDONLY)) == -1) {
            char error_msg[BUFFSIZ];
            snprintf(error_msg, sizeof(error_msg), "-1 %s\n", strerror(errno));

            write_all(newsock, error_msg, strlen(error_msg));
            close(fd_file_read);
            
            return -1;
        }
        
        off_t file_size = lseek(fd_file_read, 0, SEEK_END);
        lseek(fd_file_read, 0, SEEK_SET);

        char size_buf[BUFFSIZ / 4];
        int len = snprintf(size_buf, sizeof(size_buf), "%ld ", file_size);
        if (write_all(newsock, size_buf, len) == -1) {
            #ifdef DEBUG
            perror("Write size");
            #endif
            
            close(fd_file_read);
            return -1;
        }
        
        ssize_t read_bytes;
        char buffer[BUFFSIZ];
        while ((read_bytes = read(fd_file_read, buffer, sizeof(buffer))) > 0) {
            if (write_all(newsock, buffer, read_bytes) == -1) {
                #ifdef DEBUG
                perror("Write full");
                #endif
                
                close(fd_file_read);
                return -1;;
            }
        }

        close(fd_file_read);
        return 0;
    }

    if (!cmd.chunk_size && cmd.op == PUSH) return 0; // final chunk, nothing to write

    // just create the file if chunk size is -1
    if (cmd.op == PUSH && cmd.chunk_size == -1) {
        int fd_file_write = open(cmd.path, O_WRONLY | O_TRUNC | O_CREAT, 0666);
        
        if (fd_file_write == -1) {
            #ifdef DEBUG
            perror("push open\n");
            #endif
            return 1;
        }

        close(fd_file_write);
        return 0;
    }

    if (cmd.op == PUSH) {
        // Read the data because we send: PUSH file size\r\ndata
        // important because data might have \r\n in them
        ssize_t bytes_read = 0;
        if ((bytes_read = read_all(newsock, cmd.data, cmd.chunk_size)) < 0) {
            #ifdef DEBUG
            perror("read exec command for push");
            #endif
            return 1;
        }


        int fd_file_write = open(cmd.path, O_WRONLY | O_APPEND);
        
        if (fd_file_write == -1) {
            #ifdef DEBUG
            perror("push open\n");
            #endif
            return 1;
        }

        ssize_t write_bytes = write(fd_file_write, cmd.data, cmd.chunk_size);
        if (write_bytes != cmd.chunk_size) {
            #ifdef DEBUG
            perror("Invalid written bytes");
            #endif
            return 1;
        }

        close(fd_file_write);
        return  0;
    }
    
    // Invalid command
    return 1;
}


void* handle_connection_th(void* arg)
{
    int newsock = *(int*)arg;
    free(arg);

    ssize_t n_read;
    char read_buffer[BUFFSIZ];
    memset(read_buffer, 0, sizeof(read_buffer));

    while ((n_read = read_command_from_manager(newsock, read_buffer, sizeof(read_buffer))) > 0) {
        client_command current_cmd_struct;

        if(parse_manager_command(read_buffer, &current_cmd_struct)){
            #ifdef DEBUG
            fprintf(stderr, "error: parse_command [%s]\n", read_buffer);
            #endif
            break;
        }
        if (exec_cmd_client(current_cmd_struct, newsock)) {
            #ifdef DEBUG
            fprintf(stderr, "error: exec_cmd_client\n\n");
            #endif
            break;
        }
        memset(read_buffer, 0, sizeof(read_buffer));
    }

    close(newsock);
    return NULL;
}