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

extern __thread int sockDesc ;

extern __thread int myFileDesc ;



void serverMessageFunction(LOGIN_MESSAGE *logMsgPtr) {

    while (sem_wait(&listSemArray[1]))
        retryProc("MSG : Errore presa token lettura lista\n") ;

    CLIENT_NODE *myClientNode = treeSearchNode(logMsgPtr->userName) ;

    if (sem_post(&listSemArray[1]))
        exitProc("Errore rilascio token lettura lista\n") ;


    char myFileName[BUFF_SIZE] ;
    sprintf(myFileName , "./serverDirectory/%s_messages" , myClientNode->userName) ;
    myFileDesc = open(myFileName , O_RDWR) ;
    if (myFileDesc == -1) {
        exitProc("MSG : Errore apertura file client\n") ;
    } 

    int msgType ;
    int ret ;
    HEADER msgHeader ;

    readFromSocket((char *)&msgHeader , 0, MSG_HEADER_SIZE) ;
    msgType = msgHeader.type = (int)ntohl(msgHeader.type) ;
    
    while (msgType != EXIT_TYPE) {

        switch (msgType) {

            case SEND_TYPE :
                ret = serverSendFunction(&msgHeader) ;
                break ;

            case READ_TYPE :
                ret = serverReadFunction(&msgHeader , myClientNode) ;
                break ;

            case DEL_TYPE :
                ret = serverDeleteFunction(&msgHeader , myClientNode) ;
                break ;
        }

        //printf("ret %d\n" , ret) ;
        answerToClient(ret) ;

        readFromSocket((char *)&msgHeader , 0, MSG_HEADER_SIZE) ;
        msgType = msgHeader.type = (int)ntohl(msgHeader.type) ;
    }

    serverExitFunction() ;
}



int serverSendFunction(HEADER *msgHeaderPtr) {

    SEND_MESSAGE sendMsg ;
    sendMsg.header.type = msgHeaderPtr->type ;

    readFromSocket((char *)&sendMsg , MSG_HEADER_SIZE , SEND_MSG_SIZE) ;


    while(sem_wait(&listSemArray[1]))
        retryProc("SEND : Errore presa token lettura list\n") ;
    
    
    CLIENT_NODE *destNode = treeSearchNode(sendMsg.destUser) ;
    

    if (sem_post(&listSemArray[1]))
        exitProc("SEND : Errore rilascio token lettura\n") ;

    if (destNode == NULL) 
        return NEG_TYPE_1 ;



    while (pthread_mutex_lock(&(destNode->clientMutex)))
        retryProc("SEND : Errore presa mutex dest\n") ;


    char destFileName[BUFF_SIZE] ;
    sprintf(destFileName , "./serverDirectory/%s_messages" , destNode->userName) ;

    int destFileDesc = open(destFileName , O_RDWR) ;
    if (destFileDesc == -1)
        exitProc("SEND : Errore apertura file dest\n") ;

    CLIENT_FILE_HEADER destFileHeader ;

    if (lseek(destFileDesc , 0 , SEEK_SET) == -1)
        exitProc("SEND : Errore riposizinamento inizale\n") ;

    if (read(destFileDesc , &destFileHeader , FILE_HEADER_SIZE) == -1)
        exitProc("SEND : Errore lettura file header\n") ;

    
    if (destFileHeader.totMsg == MAX_MSG_NUM) {
        //Il file di dest è pieno
        close(destFileDesc) ;
        if (pthread_mutex_unlock(&(destNode->clientMutex)))
            exitProc("SEND : Errore rilascio mutex dest\n") ;
        return NEG_TYPE_2 ;
    }


    int i ;
    for (i = 0 ; i < MAX_MSG_NUM ; i++) {
        if (destFileHeader.freePos[i] == 1)
            break ;
    }

    
    if (lseek(destFileDesc , 0 , SEEK_SET) == -1)
        exitProc("SEND : Errore riposizionamento file dest\n") ;

    destFileHeader.freePos[i] = 0 ;

    sendMsg.header.msgNum = i ;
    
    memmove(&destFileHeader.startMessagePos[1] , &destFileHeader.startMessagePos[0], sizeof(int) * destFileHeader.totMsg) ;
    destFileHeader.startMessagePos[0] = i ;

    destFileHeader.totMsg++ ;

    if (write(destFileDesc , &destFileHeader , FILE_HEADER_SIZE) == -1)
        exitProc("SEND : Errore scrittura header aggiornato\n") ;

    if (lseek(destFileDesc , FILE_HEADER_SIZE + i * SEND_MSG_SIZE, SEEK_SET) == -1)
        exitProc("SEND : Errore riposizionamento file dest\n") ;

    if (write(destFileDesc , &sendMsg , SEND_MSG_SIZE) == -1)
        exitProc("SEND : Errore scrittura nuovo messaggio\n") ;
    

    if (pthread_mutex_unlock(&(destNode->clientMutex)))
        exitProc("SEND : Errore rilascio mutex dest\n") ;


    close(destFileDesc) ;

    return POS_TYPE ;
}



int serverReadFunction(HEADER *msgHeaderPtr , CLIENT_NODE *myClientNode) {

    READ_MESSAGE readMsg ;
    readMsg.header.type = msgHeaderPtr->type ;

    readFromSocket((char *)&readMsg , MSG_HEADER_SIZE , READ_MSG_SIZE) ;


    while (pthread_mutex_lock(&(myClientNode->clientMutex)))
        exitProc("READ : Errore presa mutex per lettura\n") ;

    if (lseek(myFileDesc , 0, SEEK_SET) == -1)
        exitProc("READ : Errore riposizionamento per lettura\n") ;
    
    CLIENT_FILE_HEADER fileHeader ;

    if (read(myFileDesc , &fileHeader , FILE_HEADER_SIZE) == -1)
        exitProc("READ : Errore lettura header del file\n") ;

    
    int readAll , readSource , readObject ;

    if (strlen(readMsg.object) == 0 && strlen(readMsg.sourceUser) == 0) {
        readAll = 1 ;
        readSource = readObject = 0 ;
    }

    if (strlen(readMsg.object) != 0) {
        readAll = readSource = 0 ;
        readObject = 1 ;
    }

    if (strlen(readMsg.sourceUser) != 0) {
        readAll = readObject = 0 ;
        readSource = 1 ;
    }

    
    int i = 0 ;
    int j = 0 ;
    SEND_MESSAGE msgToSendArray[fileHeader.totMsg] ;
    while (i < fileHeader.totMsg) {

        int msgIndex = fileHeader.startMessagePos[i] ;

        if (lseek(myFileDesc , FILE_HEADER_SIZE + msgIndex * SEND_MSG_SIZE , SEEK_SET) == -1)
            exitProc("READ : Errore riposizionamento per lettura file\n") ;


        if (read(myFileDesc , &msgToSendArray[j] , SEND_MSG_SIZE) == -1)
            exitProc("READ : Errore lettura messaggio\n") ;

        msgToSendArray[j].header.type = (int)htonl(msgToSendArray[j].header.type) ;
        msgToSendArray[j].header.msgNum = (int)htonl(msgToSendArray[j].header.msgNum) ;
        
        i++ ;
        j++ ;
    }


    if (pthread_mutex_unlock(&(myClientNode->clientMutex)))
        exitProc("READ : Errore rilascio mutex\n") ;


    for (i = 0 ; i < fileHeader.totMsg ; i++) {
        if (readAll)
            writeOnSocket((char *)&msgToSendArray[i] , 0, SEND_MSG_SIZE) ;
        
        if (readObject && strcmp(msgToSendArray[i].object , readMsg.object) == 0) 
            writeOnSocket((char *)&msgToSendArray[i] , 0, SEND_MSG_SIZE) ;

        if (readSource && strcmp(msgToSendArray[i].sendUser , readMsg.sourceUser) == 0)
            writeOnSocket((char *)&msgToSendArray[i] , 0, SEND_MSG_SIZE) ;
    }


    return POS_TYPE ;
}



int serverDeleteFunction(HEADER *msgHeader , CLIENT_NODE *myClientNode) {

    DEL_MESSAGE delMsg ;
    delMsg.header.type = msgHeader->type ;
    delMsg.header.msgNum = (int)ntohl(msgHeader->msgNum) ;

    int delNum = delMsg.header.msgNum ;

    while (pthread_mutex_lock(&(myClientNode->clientMutex)))
        retryProc("DEL : Errore presa mutex file\n") ;

    if (lseek(myFileDesc , 0, SEEK_SET) == -1)
        exitProc("DEL : Errore riposizionamento a inizio file\n") ;

    CLIENT_FILE_HEADER fileHeader ;

    if (read(myFileDesc , &fileHeader , FILE_HEADER_SIZE) == -1)
        exitProc("DEL : Errore lettura header dal file\n") ;


    int i ;
    for (i = 0 ; i < fileHeader.totMsg ; i++)
        if (fileHeader.startMessagePos[i] == -1 || fileHeader.startMessagePos[i] == delNum)
            break ;

    if (fileHeader.startMessagePos[i] == delNum) {

        fileHeader.freePos[delNum] = 1 ;
        memmove(&fileHeader.startMessagePos[i] , &fileHeader.startMessagePos[i+1] , (fileHeader.totMsg-1-i) * sizeof(int)) ;

        fileHeader.totMsg-- ;

        fileHeader.startMessagePos[fileHeader.totMsg] = -1 ;


        if (lseek(myFileDesc , 0, SEEK_SET) == -1)
            exitProc("DEL : Errore riposizionamento per riscrittura header\n") ;

        if (write(myFileDesc , &fileHeader , FILE_HEADER_SIZE) == -1)
            exitProc("DEL : Errore riscrittura header\n") ;
    }

    if (pthread_mutex_unlock(&(myClientNode->clientMutex)))
        exitProc("DEL : Errore rilascio mutex\n") ;

    return POS_TYPE ;

}



CLIENT_NODE *treeSearchNode(char *userName) {

    CLIENT_NODE *curr = treeRoot ;

    while (curr != NULL) {
        if (strcmp(userName , curr->userName) == 0)
            return curr ;
        
        else if (strcmp(userName , curr->userName) < 0)
            curr = curr->leftChild ;
        
        else
            curr = curr->rightChild ;
    }

    return NULL ;
}
