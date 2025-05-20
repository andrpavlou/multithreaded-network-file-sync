#include "nfs_manager.h"
#include "utils.h"
#include "sync_task.h"
#include "sync_info.h"


int check_args(int argc, char *argv[], char** logfile, char** config_file, int *worker_limit, int *port, int *buffer_size){
    int found = 0;
    for(int i = 1; i < argc; i++){
        if(strncmp(argv[i], "-l", 2) == 0 && i + 1 < argc){
            *logfile = argv[++i];
        }

        else if(!strncmp(argv[i], "-c", 2) && i + 1 < argc){
            *config_file = argv[++i];
        }

        else if(!strncmp(argv[i], "-n", 2) && i + 1 < argc){
            *worker_limit = atoi(argv[++i]);
            found = 1;
        }

        else if(!strncmp(argv[i], "-p", 2)  && i + 1 < argc){

            *port = atoi(argv[++i]);
        }

        else if(!strncmp(argv[i], "-b", 2) && i + 1 < argc)
            *buffer_size = atoi(argv[++i]);
        
        else return 1;
    }

    *worker_limit = found == 0 ? 5 : *worker_limit;

    return 0;
}



// bin/nfs_manager -l logs/manager.log -c config.txt -n 5 -p 2345 -b 512
int main(int argc, char *argv[]){
    char *logfile       = NULL;
    char *config_file   = NULL;
    
    int port            = -1;
    int worker_limit    = -1;
    int buffer_size     = -1;

    if(check_args(argc, argv, &logfile, &config_file, &worker_limit, &port, &buffer_size)){
        perror("USAGE: ./nfs_manager -l <logfile> -c <config_file> -n <worker_limit> -p <port> -b <buffer_size>");
        return 1;
    }

    printf("logfile: %s\n", logfile);
    printf("config_file: %s\n", config_file);
    printf("worker_limit: %d\n", worker_limit);
    printf("port: %d\n", port);
    printf("buffer_size: %d\n", buffer_size);

    // Config file parsing
    int fd_config;
    if((fd_config = open(config_file, O_RDONLY)) < 0){
        perror("Config file open problem");
        exit(3);
    }

    int total_config_pairs = line_counter_of_file(fd_config, BUFFSIZ);

    lseek(fd_config, 0, SEEK_SET); // Reset the pointer at the start of the file

    config_pairs *conf_pairs = malloc(sizeof(config_pairs) * total_config_pairs);
    create_cf_pairs(fd_config, conf_pairs);
    
    for (int i = 0; i < total_config_pairs; i++) {
        printf("Pair: %d\n", i);
        printf("  Raw Source:\t\t%s\n", conf_pairs[i].source_full_path);
        printf("  Parsed Source Path:\t%s\n", conf_pairs[i].source_dir_path);
        printf("  Parsed Source IP:\t%s\n", conf_pairs[i].source_ip);
        printf("  Parsed Source Port:\t%d\n", conf_pairs[i].source_port);

        printf("  Raw Target:\t\t%s\n", conf_pairs[i].target_full_path);
        printf("  Parsed Target Path:\t%s\n", conf_pairs[i].target_dir_path);
        printf("  Parsed Target IP:\t%s\n", conf_pairs[i].target_ip);
        printf("  Parsed Target Port:\t%d\n", conf_pairs[i].target_port);
        printf("--------------------------------------------------\n");
    }


    
    return 0;
}