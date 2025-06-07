#define _GNU_SOURCE

#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>

#include "utils.h"
#include "client_connection_handler.h"

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


static void handle_sigint(int sig){
    client_active = 0;
}


int main(int argc, char* argv[]){
    int port = 0;   
    if(check_args_client(argc, (const char**)argv, &port)){
        perror("USAGE: ./nfs_client -p <port_number>");
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
    if((sock = socket(AF_INET , SOCK_STREAM , 0)) < 0){
        #ifdef DEBUG
        perror("socket");
        #endif
        
    }
    
    int optval = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    server.sin_family       = AF_INET ; /* Internet domain */
    server.sin_addr.s_addr  = htonl(INADDR_ANY);
    server.sin_port         = htons(port) ; /* The given port */
    
    
    /* Bind socket to address */
    if(bind(sock, serverptr, sizeof(server)) < 0){
        #ifdef DEBUG
        perror("bind");
        #endif
        
        return 1;
    }

    if(listen(sock, 5) < 0){
        #ifdef DEBUG
        perror (" listen ");
        #endif
    }
    #ifdef DEBUG
    printf("Listening for connections to port % d \n", port);
    #endif

    socklen_t clientlen;
    while(client_active){
        clientlen = sizeof(struct sockaddr_in);

        int temp_sock;
        if((temp_sock = accept(sock, clientptr, &clientlen)) < 0){
            #ifdef DEBUG
            perror("accept ");
            #endif
            continue;
        }
        #ifdef DEBUG
        printf("Accepted connection \n") ;
        #endif
        
        int *newsock = malloc(sizeof(int));
        *newsock = temp_sock;

        pthread_t client_th;
        if(pthread_create(&client_th, NULL, handle_connection_th, newsock) != 0){
            #ifdef DEBUG
            perror("pthread_create");
            #endif
            
            close(*newsock);
            free(newsock);
            continue;
        }
        pthread_detach(client_th);
    }
    close(sock);

    return 0;
}

