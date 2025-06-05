#include <sys/types.h>
#include <sys/stat.h>   
#include <unistd.h>     
#include <stdlib.h>     
#include <string.h>
#include <netdb.h>      
#include <stdio.h>      
#include <errno.h>
#include <time.h>
#include <ctype.h>

#include "utils.h"


static inline int create_dir(const char *path){
    struct stat st = {0};

    if(stat(path, &st) == -1){
        return mkdir(path, 0700); // read, write to the owner perms
    }
    
    return 0;
}

int read_next_line_from_fd(int fd, char *line, int max_len){
    static char buffer[BUFFSIZ];
    static int buffer_pos = 0;
    static int buffer_len = 0;

    int line_pos = 0;

    do{
        if(buffer_pos >= buffer_len){
            buffer_len = read(fd, buffer, sizeof(buffer));
            buffer_pos = 0;

            // return case if the file doesnt end with \n
            if(!buffer_len && line_pos > 0){
                line[line_pos] = '\0';
                return line_pos;
            }

            if(!buffer_len){
                return 0;
            }
            
        }

        char current_char = buffer[buffer_pos++];
        if(line_pos < max_len - 1){
            line[line_pos++] = current_char;
        }

        if(current_char == '\n'){
            break;
        }
    } while(buffer_len > 0);


    line[line_pos] = '\0';
    return line_pos;
}

int line_counter_of_file(int fd, int max_len){
    static char buffer[BUFFSIZ];
    static int buffer_pos = 0;
    static int buffer_len = 0;

    int line_counter     = 0;
    int last_char_was_nl = 1; // flag if the file ends with \n

    do{
        if(buffer_pos >= buffer_len){
            buffer_len = read(fd, buffer, sizeof(buffer));
            buffer_pos = 0;
            
            // eof, return number of lines
            if(buffer_len == 0) break;
        }

        char current_char = buffer[buffer_pos++];


        if(current_char == '\n'){
            line_counter++;
            last_char_was_nl = 1;
        } else {
            last_char_was_nl = 0;
        }

    } while(buffer_len > 0);


    return line_counter + (last_char_was_nl ^ 1); // add + 1 if the file doesnt end with \n
}


int parse_path(const char *full_path, char *dir_path, char *ip, int *port){
    // full_path example: /source1@127.0.0.1:2323

    // pointer -> @127.0.0.1:2323
    const char *at = strchr(full_path, '@');
    if(at == NULL) return -1;

    // pointer -> :2323
    const char *colon = strchr(at, ':');
    if(colon == NULL) return -1;

    /* /source1@127.0.0.1:2323
    *  |<----->|
    */ 

    if(dir_path != NULL){
        size_t path_len = at - full_path;
        strncpy(dir_path, full_path, path_len);
        dir_path[path_len] = '\0';
    }

    /* /source1@127.0.0.1:2323
    *          |<------->|
    */ 
    size_t ip_len = colon - (at + 1);
    strncpy(ip, at + 1, ip_len);
    ip[ip_len] = '\0';


    // :PORT -> (:) + 1
    *port = atoi(colon + 1);
    return 0;
}



int create_cf_pairs(int fd_config, config_pairs *conf_pairs){
    char line[BUFFSIZ * 2];
    int curr_line = 0;
    
    // parse the config file line by line 
    while(read_next_line_from_fd(fd_config, line, sizeof(line)) > 0) {

        line[strcspn(line, "\n")] = '\0'; // replace the newlines with '\0'

        // src dir and path are split with a space
        char* src_dir       = strtok(line, " ");
        char* target_dir    = strtok(NULL, " ");

        strncpy(conf_pairs[curr_line].source_full_path, src_dir, BUFFSIZ - 1);
        strncpy(conf_pairs[curr_line].target_full_path, target_dir, BUFFSIZ - 1);

        conf_pairs[curr_line].source_full_path[BUFFSIZ - 1] = '\0';
        conf_pairs[curr_line].target_full_path[BUFFSIZ - 1] = '\0';

        parse_path(conf_pairs[curr_line].source_full_path,
                  conf_pairs[curr_line].source_dir_path,
                  conf_pairs[curr_line].source_ip,
                  &conf_pairs[curr_line].source_port);


        parse_path(conf_pairs[curr_line].target_full_path,
                  conf_pairs[curr_line].target_dir_path,
                  conf_pairs[curr_line].target_ip,
                  &conf_pairs[curr_line].target_port);

        // if(create_dir(conf_pairs[curr_line].source)){
        //     perror("error creating directory source directory\n");
        //     return 1;
        // }

        // if(create_dir(conf_pairs[curr_line].target)){
        //     perror("error creating directory target directory\n");
        //     return 1;
        // }

        curr_line++;
    }
    return 0;
}


int check_args_manager(int argc, char *argv[], char** logfile, char** config_file, int *worker_limit, int *port, int *buffer_size){
    bool found_worker_limit = FALSE;

    for(int i = 1; i < argc; i++){
        if(!strncmp(argv[i], "-l", 2) && (i == 1) && (i + 1 < argc)){
            *logfile = argv[++i];
        }

        else if(!strncmp(argv[i], "-c", 2) && (i == 3) && (i + 1 < argc)){
            *config_file = argv[++i];
        }

        else if(!strncmp(argv[i], "-n", 2) && (i == 5) && (i + 1 < argc)){
            *worker_limit = atoi(argv[++i]);
            found_worker_limit = TRUE;
        }

        else if(!strncmp(argv[i], "-p", 2)  && ((i == 7) || (i == 5 && found_worker_limit == FALSE)) && (i + 1 < argc)){
            *port = atoi(argv[++i]);
        }

        else if(!strncmp(argv[i], "-b", 2) && ((i == 9) || (i == 7 && found_worker_limit == FALSE)) && (i + 1 < argc)){
            *buffer_size = atoi(argv[++i]);
        }
        
        else return 1;
    }

    if(*buffer_size <= 0) return -1;
    
    *worker_limit = found_worker_limit == FALSE ? 5 : *worker_limit;

    return 0;
}



int check_args_console(int argc, char* argv[], char **logfile, char **host_ip, int *port){
    for(int i = 1; i < argc; i++){
        if(!strncmp(argv[i], "-l", 2) && (i == 1) && (i + 1 < argc)){
            *logfile = argv[++i];
        }

        else if(!strncmp(argv[i], "-h", 2) && (i == 3) && (i + 1 < argc)){
            *host_ip = argv[++i];
        }

        else if(!strncmp(argv[i], "-p", 2) && (i == 5) && (i + 1 < argc)){
            *port = atoi(argv[++i]);
        }

        else return 1;
    }
    return 0;
}


int parse_console_command(const char* buffer, manager_command *full_command, char **source_full_path, char **target_full_path){

    if(!strncasecmp(buffer, "CANCEL", 6)){
        full_command->op = CANCEL;

        buffer += 6; // Move past cancel "cancel"
        while(isspace(*buffer)) buffer++; // Skip spaces between CANCEL and PATH in case more exist
        
        char temp[BUFSIZ];
        strncpy(temp, buffer, BUFSIZ - 1);
        temp[strcspn(temp, "\n")] = '\0';   // remove newline


        char *src_token = strtok(temp, " ");
        // If the current first_token is null or more text exists after first_token with spaces we should return error
        if(src_token == NULL || strtok(NULL, " ") != NULL) return 1;
        (*source_full_path) = strdup(src_token);

        strncpy(full_command->cancel_dir, (*source_full_path), BUFFSIZ - 1);

        full_command->source_ip[BUFFSIZ - 1] = '\0';
        full_command->target_ip[0] = '\0';

        (*target_full_path) = NULL;
        return 0;
    }


    if(!strncasecmp(buffer, "ADD", 3)){
        full_command->op = ADD;

        buffer += 3;                        // Move past cancel "add"
        while(isspace(*buffer)) buffer++;   // Skip spaces between add and source in case more exist
        
        char temp[BUFSIZ];
        strncpy(temp, buffer, BUFSIZ - 1);
        temp[strcspn(temp, "\n")] = '\0';   // remove newline


        char *src_token = strtok(temp, " ");  // Source path
        if(src_token == NULL) return 1;
        *source_full_path = strdup(src_token);


        int status = parse_path((*source_full_path), full_command->source_dir, full_command->source_ip, &full_command->source_port);
        if(status == -1){
            perror("parse path target");
        }
        full_command->source_ip[BUFFSIZ - 1] = '\0';

        
        char *tgt_token = strtok(NULL, " "); // Target path
        if(tgt_token == NULL){
            free(*source_full_path);
            return 1;  
        }
        *target_full_path = strdup(tgt_token);

        
        parse_path((*target_full_path), full_command->target_dir, full_command->target_ip, &full_command->target_port);
        full_command->target_ip[BUFFSIZ - 1] = '\0';

        char *end = strtok(NULL, " ");  // If the user has given extra arguments, the command is not accepted
        if(end != NULL) return 1;
        
        return 0;
    }

    if(!strncasecmp(buffer, "SHUTDOWN", 8)){
        full_command->op = SHUTDOWN;

        full_command->source_ip[0] = '\0';
        full_command->target_ip[0] = '\0';

        return 0;
    }

    return 1;
}

//////////// TODO: CREATE MORE MODULES FOR THESE
// Handles the errors -> no need for closing the socket after error
int establish_connection(int *sock, const char *ip, const int port){
    struct sockaddr_in servadd; /* The address of server */
    struct hostent *hp;         /* to resolve server ip */

    if((*sock = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("socket");
        return 1;
    }
    
    if((hp = gethostbyname(ip)) == NULL){
        perror("gethostbyname"); 
        close(*sock);
        return 1;
    }

    memcpy(&servadd.sin_addr, hp->h_addr, hp->h_length);
    servadd.sin_port = htons(port); /* set port number */
    servadd.sin_family = AF_INET;   /* set socket type */

    if(connect(*sock, (struct sockaddr*) &servadd, sizeof(servadd)) != 0){
        perror("connect");
        close(*sock);
        return 1;
    }
    return 0;
}

ssize_t write_all(int fd, const void *buf, size_t size) {
    ssize_t total_written = 0;
    while(total_written < size){
        ssize_t n = write(fd, (char*)buf + total_written, size - total_written);
        
        if(n < 0)   return -1;
        if(n == 0)  break;
        
        total_written += n;
    }

    return total_written;
}


ssize_t read_all(int fd, void *buf, size_t size){
    ssize_t total_read_b = 0;

    while(total_read_b < size){
        ssize_t n = read(fd, (char*) buf + total_read_b, size - total_read_b);
        
        if(n <= 0) return n;
        if(n == 0) return -1;

        total_read_b += n;
    }

    return total_read_b;
}



int parse_pull_read(const char *rec_buff, ssize_t *rec_file_len, char **file_content){
    char temp_buff[BUFFSIZ];
    strncpy(temp_buff, rec_buff, sizeof(temp_buff) - 1);
    temp_buff[sizeof(temp_buff) - 1] = '\0';

    char *token = strtok(temp_buff, " ");
    if(!token) return 1;

    *rec_file_len = atoi(token);
    if(*rec_file_len == -1){
        perror("File not found from client");
        return 1;
    }

    char *first_space = strchr(rec_buff, ' ');
    if(first_space == NULL) return 1;

    *file_content = first_space + 1;

    return 0;
}


int read_list_response(int sock, char **list_reply_buff){
    size_t total_read_list      = 0;
    size_t list_buff_capacity   = 0;

    do{
        // Allocate more memory for buffer if there is not enough space
        if(list_buff_capacity - total_read_list < BUFFSIZ){
            list_buff_capacity += BUFFSIZ;
            *list_reply_buff = realloc(*list_reply_buff, list_buff_capacity);

            if(*list_reply_buff == NULL){
                perror("Realloc");
                return 1;
            }
            (*list_reply_buff)[list_buff_capacity - 1] = '\0';
        }
        ssize_t n_read = read(sock, *list_reply_buff + total_read_list, BUFFSIZ);
        
        if(n_read <= 0){
            *((*list_reply_buff) + total_read_list - 1) = '\0';
            return 1;
        }

        total_read_list += n_read;
        (*list_reply_buff)[total_read_list] = '\0';
    } while(strstr((*list_reply_buff), "\n.") == NULL); // Keep reading until we receive '.\n'

    return 0;
}

int get_files_from_list_response(char *list_reply_buff, char *file_buff[MAX_FILES]){
    int total_src_files = 0;
    char *curr_file = strtok(list_reply_buff, "\n");
    
    file_buff[total_src_files] = curr_file;
    while(curr_file && strcmp(curr_file, ".") != 0 && total_src_files < MAX_FILES){
        total_src_files++;
        
        curr_file = strtok(NULL, "\n");
        file_buff[total_src_files] = curr_file;
    }

    return total_src_files;
}

long get_file_size_of_host(int sock){
    int ind = 0;
    char space_ch;
    char num_buff[32];

    // Read the size of the file we are about to send
    while(read(sock, &space_ch, 1) == 1){
        if(space_ch == ' ') break;
        
        num_buff[ind++] = space_ch;
    }
    num_buff[ind] = '\0';

    char *end;
    long file_size = strtol(num_buff, &end, 10);
    if(end == num_buff || *end != '\0') return -1;

    return file_size;
}   



// returns current time and date formatted string
char* get_current_time_str(void){

    static char time_buffer[64];        // static is required to be able to return it.
    time_t current_time = time(NULL);   // get current time
    struct tm *time_info = localtime(&current_time); // update the struct with the  current time 

    if(time_info != NULL){
        strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", time_info);
    } else {
        snprintf(time_buffer, sizeof(time_buffer), "Unknown time");
    }

    return time_buffer;
}

