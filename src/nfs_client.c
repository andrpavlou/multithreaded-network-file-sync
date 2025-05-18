#include "nfs_client.h"


/*
  socket()
    |
  bind()
    |
  listen()
    |
  accept()
    |
(wait for connection)
    |
  read()
    |
(processing)
    |
  write()
*/


volatile sig_atomic_t client_active = 1;


int arg_check(int argc, const char** argv, int *port){
    if(argc != 3) return 1;

    if(strncmp(argv[1], "-p", 2)) return 1;

    *port = atoi(argv[2]);
    return 0;
}


const char* cmd_to_str(command cmd) {
    if(cmd == LIST) return "LIST";
    if(cmd == PULL) return "PULL";
    if(cmd == PUSH) return "PUSH";
    
    return "INVALID";
}

// TODO: struct with both command/dir to parse it completely
command parse_command(const char* buffer){
    if(!strncmp(buffer, "LIST", 4)) return LIST;
    if(!strncmp(buffer, "PULL", 4)) return PULL;
    if(!strncmp(buffer, "PUSH", 4)) return PUSH;

    return INVALID;
}
void handle_sigint(int sig) {
    client_active = 0;
}

int main(int argc, char* argv[]){
    int port = 0;   
    if(arg_check(argc, (const char**)argv, &port)){
        perror("USAGE");
        return 1;
    }

    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; 
    sigaction(SIGINT, &sa, NULL);


    struct sockaddr_in server, client;
    struct sockaddr *serverptr = (struct sockaddr *) &server;
    struct sockaddr *clientptr = (struct sockaddr *) &client;
    

    int sock = 0;
    if((sock = socket (AF_INET , SOCK_STREAM , 0)) < 0)
        perror("socket");

    server.sin_family = AF_INET ; /* Internet domain */
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port) ; /* The given port */
    /* Bind socket to address */

    if(bind(sock, serverptr, sizeof(server)) < 0)
        perror("bind") ;

    if(listen(sock, 5) < 0)
        perror (" listen ") ;
    
    socklen_t clientlen;
    printf("Listening for connections to port % d \n", port);
    while(client_active){
        clientlen = sizeof(struct sockaddr_in);

        int newsock = 0;
        if((newsock = accept(sock, clientptr, &clientlen)) < 0) 
            perror("accept ");

        printf("Accepted connection \n") ;

        char buffer[BUFSIZ];
        ssize_t n = read(newsock, buffer, sizeof(buffer) - 1);
        
        if(n < 0) perror("read");
        
        buffer[n] = '\0';
        
        command curr_cmd = parse_command(buffer);
        printf("Message Received: %sWith command: %s\n", buffer, cmd_to_str(curr_cmd));

        
        close(newsock);
    }
    close(sock);
    printf("Exiting\n");


    return 0;
}