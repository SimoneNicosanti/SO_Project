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
#include <sys/ipc.h>
#include <sys/sem.h>
#include <poll.h>
#include <signal.h>


#include "serverDeclaration.h"


#define MY_IP_ADDR INADDR_ANY
#define MY_PORT_NUM 49152


sem_t fileSemArray[2] ;
sem_t listSemArray[2] ;
int clientsInfoDesc ;

CLIENT_NODE *treeRoot = NULL ;


void exitProc(char *exitMessage) {
    printf("%s" , exitMessage) ;
    printf("%s\n\n" , strerror(errno)) ;
    exit(-1) ;
}

void retryProc(char *exitMessage) {
    if (errno == EINTR) {
        errno = 0 ;
        return ;
    }
    exitProc(exitMessage) ;
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
    printf(" Server Avviato ") ;
    showFormat('*') ;
    printf("\n") ;
    printf("Premere Control+C per Uscire oppure inviare segnale SIGKILL a pid %d\n\n", getpid()) ;

    fflush(stdout) ;

    serverSignalSet() ;

    //Creo nuova directory per i file che il server deve gestire
    if (mkdir("serverDirectory", S_IRWXU)) {
        if (errno != EEXIST)
            exitProc("Errore creazione directory server\n") ;
    }


    if (errno == EEXIST)
        errno = 0 ; 


    // Nuovo socket per la comunicazione: ci faccio la bind e invoco la listen.
    int sockDesc = socket(AF_INET, SOCK_STREAM, 0) ;
    if (sockDesc == -1)
        exitProc("Errore creazione socket server\n") ;

    
    struct sockaddr_in sockAddr ;
    struct in_addr ipAddrStruct ;
    sockAddr.sin_family = AF_INET ;
    sockAddr.sin_port = (in_port_t)htons(MY_PORT_NUM) ;

    ipAddrStruct.s_addr = MY_IP_ADDR ;


    sockAddr.sin_addr = ipAddrStruct ;

    if (bind(sockDesc, (struct sockaddr *)&sockAddr, (socklen_t)sizeof(struct sockaddr_in)))
        exitProc("Errore bind del socket\n") ;

    if (listen(sockDesc , MAX_CLIENT_NUM))
        exitProc("MAIN : Errore invocazione listen\n") ;

    if (setsockopt(sockDesc , SOL_SOCKET , SO_REUSEADDR , &sockAddr , (socklen_t)sizeof(struct sockaddr_in)))
        exitProc("MAIN : Errore impostazione socket\n") ;

    printf("Eseguita Bind. Indirizzo:\n") ;
    printf("IP Number:   %d\n", MY_IP_ADDR) ;
    printf("Port Number: %d\n\n", MY_PORT_NUM) ;


    // Se non esiste creo il file per la gestione delle informazioni dei client
    clientsInfoDesc = open(CLIENTS_INFO_FILE_NAME, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR) ;
    if (clientsInfoDesc == -1)
        exitProc("Errore creazione file lista clienti\n") ;


    // Istanzio i semafori per la sincronizzazione e li inizializzo
    if (sem_init(&fileSemArray[0] , 0, 1))
        exitProc("MAIN : Errore init token scrittura file") ;

    if (sem_init(&fileSemArray[1] , 0, MAX_CLIENT_NUM))
        exitProc("MAIN : Errore init tokens lettura file\n") ;
    
    if (sem_init(&listSemArray[0] , 0, 1))
        exitProc("MAIN : Errore init token scrittura list\n") ;

    if (sem_init(&listSemArray[1] , 0, MAX_CLIENT_NUM))
        exitProc("MAIN : Errore init tokens lettura list\n") ;


    //Inizializzo la lista dei client 
    fillClientList() ;

    pthread_t controlThread ;
    if (pthread_create(&controlThread , NULL, controlThreadFunction, NULL))
        exitProc("MAIN : Errore lancio control thread\n") ;

    
    pthread_t serverThread ;

    while (1) {
        struct sockaddr_in connectedAddr ;

        int newSockDesc ;

        socklen_t addrSize ;
        while ((newSockDesc = accept(sockDesc, (struct sockaddr *)&connectedAddr, &addrSize)) == -1)
            retryProc("Errore ricezione connessioni\n") ;


        printf("Ricevuta Connessione\n") ;
        
        int *threadParam = (int *)malloc(sizeof(int)) ;
        if (threadParam == NULL)
            exitProc("MAIN : Errore malloc nuovo descrittore\n") ;
        *threadParam = newSockDesc ;
        //printf("%d\n", newSockDesc) ;
        /*
         * connectedAddr.sin_addr.s_addr = ntohl(connectedAddr.sin_addr.s_addr) ;
         * Non ho bisogno d questa conversione : la conversione da network order a 
         * host order è fatta internamente dalla inet_ntoa
         */
        connectedAddr.sin_port = (in_port_t)ntohs(connectedAddr.sin_port) ;

        printf("IP Number:   %s\n" , inet_ntoa(connectedAddr.sin_addr)) ;
        printf("Port Number: %d\n\n", connectedAddr.sin_port) ;

        
        // Creo il thread server per il client che si è connesso
        if (pthread_create(&serverThread , NULL, serverStartFunction , threadParam))
            exitProc("MAIn : Errore lancio thread server\n") ;
    }
}



void fillClientList() {
    
    FILE *clientFileStream = fopen(CLIENTS_INFO_FILE_NAME , "r") ;
    if (clientFileStream == NULL)
        exitProc("FILL : Errore apertura stream\n") ;

    
    char *fileRow;
    int matchNum ;
    
    do {
        errno = 0 ;
        matchNum = fscanf(clientFileStream , "%m[^\n]" , &fileRow) ;
    } while (errno == EINTR) ;

    if (errno != 0)
        exitProc("FILL : Errore lettura file clients\n") ;


    while (matchNum != EOF) {

        fgetc(clientFileStream) ;

        char *currName = strtok(fileRow , "\t") ;


        CLIENT_NODE *newNode = (CLIENT_NODE *)malloc(sizeof(CLIENT_NODE)) ;
        if (newNode == NULL)
            exitProc("FILL : Errore malloc newNode\n") ;

        if (pthread_mutex_init(&(newNode->clientMutex) , NULL))
            exitProc("FILL : Errore inizializzazione mutex nodo\n") ;

        strcpy(newNode->userName , currName) ;
        newNode->leftChild = NULL ;
        newNode->rightChild = NULL ;


        treeInsertNode(newNode) ;

        char fileName[BUFF_SIZE] ;
        sprintf(fileName , "./serverDirectory/%s_messages", currName) ;

        int fileDesc = open(fileName , O_CREAT | O_EXCL | O_WRONLY , 0600) ;
        if (fileDesc != -1) {
            //Anomalia: client iscritto , file non esistente
            clientFileInit(fileDesc, ANOMALY) ;
            close(fileDesc) ;
        }
        else
            errno = 0 ;


        free(fileRow) ;


        do {
            errno = 0 ;
            matchNum = fscanf(clientFileStream , "%m[^\n]" , &fileRow) ;
        } while (errno == EINTR) ;

        if (errno != 0)
            exitProc("FILL : Errore lettura file clients\n") ;

    }

    fclose(clientFileStream) ;

    return ;
}



void treeInsertNode(CLIENT_NODE *newNode) {

    CLIENT_NODE *prev = NULL ;
    CLIENT_NODE *curr = treeRoot ;

    while (curr != NULL) {
        prev = curr ;
        
        if (strcmp(newNode->userName , curr->userName) < 0)
            curr = curr->leftChild ;
        
        else
            curr = curr->rightChild ;
    }

    if (prev == NULL)
        treeRoot = newNode ;

    else
        if (strcmp(newNode->userName , prev->userName) < 0)
            prev->leftChild = newNode ;
        else
            prev->rightChild = newNode ;
    
    return ;
}



void serverSignalSet() {

    sigset_t signalSet ;

    sigfillset(&signalSet) ;

    sigprocmask(SIG_SETMASK , &signalSet , NULL) ;

    return ;
}



void *controlThreadFunction(void *param) {

    sigset_t signalSet ;

    sigfillset(&signalSet) ;

    sigdelset(&signalSet , SIGINT) ;

    sigprocmask(SIG_SETMASK , &signalSet , NULL) ;

    
    struct sigaction signalAct ;
    memset(&signalAct , 0, sizeof(struct sigaction)) ;
    signalAct.sa_handler = sigint_handler ;
    sigfillset(&(signalAct.sa_mask)) ;

    sigaction(SIGINT, &signalAct , NULL) ;

    pause() ;
}


void sigint_handler(int signalCode) {

    printf("\nRicevuto Segnale Uscita\n") ;

    int i ;

    while (sem_wait(&fileSemArray[0]))
        retryProc("Handler : Errore presa token scrittura file\n") ;
    
    for (i = 0 ; i < MAX_CLIENT_NUM ; i++)
        while (sem_wait(&fileSemArray[1]))
            retryProc("Handler : Errore presa token lettura file\n") ;

    
    while (sem_wait(&listSemArray[0]))
        retryProc("Handler : Errore presa token scrittura file\n") ;
    
    for (i = 0 ; i < MAX_CLIENT_NUM ; i++)
        while (sem_wait(&listSemArray[1]))
            retryProc("Handler : Errore presa token lettura file\n") ;


    takeAllMutexes(treeRoot) ;

    exit(0) ;
}


void takeAllMutexes(CLIENT_NODE *treeRoot) {

    if (treeRoot == NULL)
        return ;

    else {
        while (pthread_mutex_lock(&(treeRoot->clientMutex)))
            retryProc("TAKE : Errore presa tutti i mutex\n") ;

        takeAllMutexes(treeRoot->leftChild) ;
        takeAllMutexes(treeRoot->rightChild) ;

        return ;
    }

}