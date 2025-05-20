#include "utils.h"

static inline int create_dir(const char *path){
    struct stat st = {0};

    if(stat(path, &st) == -1){
        return mkdir(path, 0700); // read, write to the owner perms
    }
    
    return 0;
}

int read_next_line_from_file(int fd, char *line, int max_len){
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


int parse_path_target(const char *full_path, char *dir_path, char *ip, int *port){
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
    size_t path_len = at - full_path;
    strncpy(dir_path, full_path, path_len);
    dir_path[path_len] = '\0';

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
    char line[BUFFSIZ];
    int curr_line = 0;
    
    // parse the config file line by line 
    while(read_next_line_from_file(fd_config, line, sizeof(line)) > 0) {
        line[strcspn(line, "\n")] = '\0'; // replace the newlines with '\0'

        // src dir and path are split with a space
        char* src_dir       = strtok(line, " ");
        char* target_dir    = strtok(NULL, " ");

        strncpy(conf_pairs[curr_line].source_full_path, src_dir, BUFFSIZ - 1);
        strncpy(conf_pairs[curr_line].target_full_path, target_dir, BUFFSIZ - 1);

        conf_pairs[curr_line].source_full_path[BUFFSIZ - 1] = '\0';
        conf_pairs[curr_line].target_full_path[BUFFSIZ - 1] = '\0';
        conf_pairs[curr_line].worker_pid = -1;

        parse_path_target(conf_pairs[curr_line].source_full_path,
                  conf_pairs[curr_line].source_dir_path,
                  conf_pairs[curr_line].source_ip,
                  &conf_pairs[curr_line].source_port);


        parse_path_target(conf_pairs[curr_line].target_full_path,
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

        conf_pairs[curr_line].worker_pid = -1;
        curr_line++; 
    }
    return 0;
}
