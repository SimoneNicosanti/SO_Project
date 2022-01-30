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

#include "serverDeclaration.h"

extern sem_t fileSemArray[2] ;
extern sem_t listSemArray[2] ;
extern int clientsInfoDesc ;

extern CLIENT_NODE *treeRoot ;

__thread int sockDesc ;
__thread int myFileDesc = -1 ;



void *serverStartFunction(void *param) {

    serverSignalSet() ;

    sockDesc = *((int *)param) ;
    free(param) ;

    int ret ;
    int readBytes ;
    int totRead ;
    HEADER msgHeader ;
    LOGIN_MESSAGE logMsg ;
    do {
        readFromSocket((char *)&msgHeader , 0 , MSG_HEADER_SIZE) ;

        msgHeader.type = (int)ntohl(msgHeader.type) ;

        if (msgHeader.type == LOGIN_TYPE || msgHeader.type == SUB_TYPE) {
            memcpy(&logMsg , &msgHeader , MSG_HEADER_SIZE) ;

            readFromSocket((char *)&logMsg , MSG_HEADER_SIZE , LOG_MSG_SIZE) ;

            logMsg.header.type = msgHeader.type ;

            if (msgHeader.type == SUB_TYPE)
                ret = serverSubscribeFunction(&logMsg) ;

            else
                ret = serverLoginFunction(&logMsg) ;
        }

        
        else if (msgHeader.type == EXIT_TYPE) {
            serverExitFunction() ;
        }

        answerToClient(ret) ;

    } while (ret < 0) ;


    serverMessageFunction(&logMsg) ;
}



void serverExitFunction() {
    printf("Chiusura\n") ;
    close(sockDesc) ;
    if (myFileDesc != -1)
        close(myFileDesc) ;
    pthread_exit(NULL) ;
}



int serverSubscribeFunction(LOGIN_MESSAGE *logMsgPtr) {

    FILE *filePtr = fopen(CLIENTS_INFO_FILE_NAME , "r+") ;
    if (filePtr == NULL)
        return NEG_TYPE_0 ;
    
    char *rowOfFile ;
    int scanRet ;

    int actualClientNum = 0 ;

    while (sem_wait(&fileSemArray[1]))
        retryProc("Errore presa token lettura file\n") ;


    do {
        errno = 0 ;
        scanRet = fscanf(filePtr , "%m[^\n]" , &rowOfFile) ;
    } while (errno == EINTR) ;

    if (errno != 0)
        exitProc("Sub : Errore lettura del file\n") ;

    while (scanRet != EOF) {
        actualClientNum++ ;

        fgetc(filePtr) ;

        char *fileUserName = strtok(rowOfFile , "\t") ;


        if (strcmp(fileUserName , logMsgPtr->userName) == 0) {
            sem_post(&fileSemArray[1]) ;
            free(rowOfFile) ;
            fclose(filePtr) ;
            return NEG_TYPE_1 ;
        }

        free(rowOfFile) ;

        do {
            errno = 0 ;
            scanRet = fscanf(filePtr , "%m[^\n]" , &rowOfFile) ;
        } while (errno == EINTR) ;

        if (errno != 0)
            exitProc("Sub : Errore lettura del file\n") ;
    }

    // Significa che non ci sono utenti con lo stesso nome
    sem_post(&fileSemArray[1]) ;

    if (actualClientNum >= MAX_CLIENT_NUM)
        return NEG_TYPE_2 ;

    while (sem_wait(&fileSemArray[0]))
        retryProc("SUB : Errore presa token scrittura file\n") ;

    int i ;
    for (i = 0 ; i < MAX_CLIENT_NUM ; i++) {
        while (sem_wait(&fileSemArray[1]))
            retryProc("SUB : Errore presa tokens lettura file\n") ;
    }


    fprintf(filePtr , "%s\t%s\n" , logMsgPtr->userName , logMsgPtr->passwd) ;

    fclose(filePtr) ;

    for (i = 0 ; i < MAX_CLIENT_NUM ; i++)
        sem_post(&fileSemArray[1]) ;

    sem_post(&fileSemArray[0]) ;


    CLIENT_NODE *newNode = (CLIENT_NODE *)malloc(sizeof(CLIENT_NODE)) ;
    if (newNode == NULL)
        exitProc("SUB : Errore creazione nuovo nodo\n") ;

    strcpy(newNode->userName , logMsgPtr->userName) ;

    if (pthread_mutex_init(&(newNode->clientMutex) , NULL))
        exitProc("SUB : Errore init mutex nuovo nodo\n") ;
    
    newNode->leftChild = NULL ;
    newNode->rightChild = NULL ;

    while (sem_wait(&listSemArray[0]))
        retryProc("SUB : Errore presa token scrittura su lista\n") ;
    
    for (i = 0 ; i < MAX_CLIENT_NUM ; i++)
        while (sem_wait(&listSemArray[1]))
            retryProc("SUB : Errore presa token lettura\n") ;


    treeInsertNode(newNode) ;


    char fileName[BUFF_SIZE] ;
    sprintf(fileName , "serverDirectory/%s_messages", logMsgPtr->userName) ;
    int fileDesc = creat(fileName , 0600) ;
    if (fileDesc == -1)
        exitProc("SUB : Impossibile creare file per nuovo client\n") ;

    clientFileInit(fileDesc , !ANOMALY) ;
    
    close(fileDesc) ;


    for (i = 0 ; i < MAX_CLIENT_NUM ; i++)
        sem_post(&listSemArray[1]) ;
    
    sem_post(&listSemArray[0]) ;


    return POS_TYPE ;

}


 
int serverLoginFunction(LOGIN_MESSAGE *logMsgPtr) {

    FILE *filePtr = fopen(CLIENTS_INFO_FILE_NAME , "r") ;
    if (filePtr == NULL)
        return NEG_TYPE_0 ;

    char *fileRow ;
    int scanRet ;

    while (sem_wait(&fileSemArray[1]))
        retryProc("LOG : Errore presa token lettura file\n") ;

    do {
        errno = 0 ;
        scanRet = fscanf(filePtr , "%m[^\n]" , &fileRow) ;
    } while (errno == EINTR) ;

    while (scanRet != EOF) {

        fgetc(filePtr) ;
        
        char *userName = strtok(fileRow , "\t") ;
        char *password = strtok(NULL , "\t") ;

        if (strcmp(userName , logMsgPtr->userName) == 0) {
            sem_post(&fileSemArray[1]) ;
            fclose(filePtr) ;
    
            if (strcmp(password , logMsgPtr->passwd) == 0) {
                free(fileRow) ;
                return POS_TYPE ;
            }

            else {
                free(fileRow) ;
                return NEG_TYPE_1 ;
            }
        }

        free(fileRow) ;

        do {
            errno = 0 ;
            scanRet = fscanf(filePtr , "%m[^\n]" , &fileRow) ;
        } while (errno == EINTR) ;

    }

    fclose(filePtr) ;
    sem_post(&fileSemArray[1]) ;

    return NEG_TYPE_2 ;
}



void readFromSocket(char *msgPtr , int totRead , int sizeToRead) {

    int readBytes ;
    do {
        //printf("Attesa lettura\n") ;
        errno = 0 ;
        readBytes = recv(sockDesc , msgPtr + totRead , sizeToRead - totRead , 0) ;
        
        struct pollfd pollStruct ;
        pollStruct.fd = sockDesc ;
        pollStruct.events = POLLRDHUP ;

        if (poll(&pollStruct , 1 , 0) == -1)
            exitProc("Errore poll\n") ;

        if ((pollStruct.revents & POLLRDHUP) && readBytes == 0) {
            printf("Connessione chiusa\n") ;
            serverExitFunction() ;
        }


        if (errno == EINTR)
            continue ;

        else
            totRead = totRead + readBytes ;

    } while (totRead < sizeToRead) ;

    return ;
}



void answerToClient(int ansType) {

    //printf("Rispondo a client\n") ;
    ANSWER_MESSAGE ansMsg ;
    ansMsg.header.type = (int)htonl(ansType) ;

    writeOnSocket((char *)&ansMsg , 0, ANS_MSG_SIZE);

    return ;
}



void writeOnSocket(char *msgPtr , int totWrite , int sizeToWrite) {

    int writeBytes ;
    do {
        errno = 0 ;

        writeBytes = send(sockDesc , msgPtr + totWrite , sizeToWrite - totWrite, MSG_NOSIGNAL) ;

        if (errno == EPIPE || errno == ENOTCONN) {
            //printf("Connessione caduta con client\n") ;
            serverExitFunction() ;
        }

        if (errno == EINTR)
            continue ;

        else
            totWrite = totWrite + writeBytes ;

    } while (totWrite < sizeToWrite) ;

    return ;
}



void clientFileInit(int fileDesc, int how) {

    CLIENT_FILE_HEADER newFileHeader ;
    memset(&newFileHeader , 0, FILE_HEADER_SIZE) ;

    int i ;
    for (i = 0 ; i < MAX_MSG_NUM ; i++) {
        newFileHeader.startMessagePos[i] = -1 ;
        newFileHeader.freePos[i] = 1 ;
    }


    if (how == ANOMALY) {

        newFileHeader.freePos[0] = 0 ;
        newFileHeader.startMessagePos[0] = 0 ;

        newFileHeader.totMsg++ ;

        SEND_MESSAGE anomalyMsg ;
        anomalyMsg.header.type = SEND_TYPE ;
        anomalyMsg.header.msgNum = 0 ;
        sprintf(anomalyMsg.sendUser , "Server") ;
        sprintf(anomalyMsg.object, "Anomalia Messaggi") ;
        sprintf(anomalyMsg.payload, "Si è verificata un'anomalia nel sistema. Sebbene lei sia un utente iscritto, all'atto dell'inizializzazione non sono stati trovati dati che la riguardano") ;

        lseek(fileDesc , FILE_HEADER_SIZE , SEEK_SET) ;

        if (write(fileDesc , &anomalyMsg, SEND_MSG_SIZE) == -1)
            exitProc("Errore scrittura anomalia\n") ;

        lseek(fileDesc , 0, SEEK_SET) ;
    }

    if (write(fileDesc , &newFileHeader, FILE_HEADER_SIZE) == -1)
        exitProc("SUB : Errore scrittura file client\n") ;
    
    return ;
}

