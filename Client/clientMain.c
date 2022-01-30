#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/if_packet.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <poll.h>
#include <signal.h>


#include "clientDeclaration.h"


#define SERVER_IP_ADDR "127.0.0.1"
#define SERVER_PORT_NUM 49152


int sockDesc = -1 ;



void retryProc(char *exitMessage) {
    if (errno == EINTR) {
        errno = 0 ;
        return ;
    }
    exitProc(exitMessage) ;
}

void exitProc(char *exitMessage) {
    printf("%s", exitMessage) ;
    printf("%s\n\n" , strerror(errno)) ;

    showFormat('*') ;
    printf("Arrivederci") ;
    showFormat('*') ;
    fflush(stdout) ;

    exit(-1) ;
}

void showFormat(char formatChar) {
    int i ;
    for (i = 0 ; i < FORMAT_SIZE ; i++) {
        putchar(formatChar) ;
    }
    return ;
}




int main() {

    showFormat('*') ;
    printf(" Messaggistica ") ;
    showFormat('*') ;
    printf("\n") ;


    clientSignalSet() ;


    sockDesc = socket(AF_INET , SOCK_STREAM , 0) ;
    if (sockDesc == -1)
        exitProc("MAIN : Errore creazione socket\n") ;
 
 
    struct sockaddr_in serverAddr ;
    struct in_addr ipServerStruct ;
    serverAddr.sin_family = AF_INET ;

    serverAddr.sin_port = (in_port_t)htons(SERVER_PORT_NUM) ;
    
    if (inet_aton(SERVER_IP_ADDR , &ipServerStruct) == -1)
        exitProc("MAIN : Errore conversione indirizzo ip\n") ;

    serverAddr.sin_addr = ipServerStruct ;

    
    if (connect(sockDesc , (struct sockaddr *)&serverAddr , (socklen_t)sizeof(struct sockaddr_in)) == -1) 
        exitProc("Errore Connessione al Server\n") ;
        
    clientStartFunction() ;

    return 0 ;
}



void clientSignalSet() {

    sigset_t signalSet ;
    sigfillset(&signalSet) ;

    sigdelset(&signalSet , SIGHUP) ;

    sigprocmask(SIG_SETMASK , &signalSet , NULL) ;

    signal(SIGHUP, clientExitFunction) ;

    return ;
}