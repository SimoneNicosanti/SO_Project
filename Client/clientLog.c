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


#define SERVER_IP_ADDR "127.0.1.1"
#define SERVER_PORT_NUM 49152


extern int sockDesc ;

char *clientUserName ;


void clientStartFunction() {
    
    char inputRow[BUFF_SIZE] ;
    int ret = NEG_TYPE_0 ;

    while (ret < 0) {

        printf("\nInserire codice opzione scelta\n") ;
        printf("%s. Login\n", LOG_CODE) ;
        printf("%s. Iscrizione\n", SUB_CODE) ;
        printf("%s. Uscita\n", EXIT_CODE) ;
        printf(">>> ") ;

        do {
            errno = 0 ;
            fgets(inputRow , BUFF_SIZE , stdin) ;
        } while (errno == EINTR) ;

        if (errno != 0) {
            printf("Errore Lettura Input\n") ;
            ret = NEG_TYPE_0 ;
            continue ;
        }

        myStrtok(inputRow , '\n') ;

        if (strlen(inputRow) == BUFF_SIZE-1)
            freeInputBuff ;


        if (strcmp(inputRow , LOG_CODE) == 0 || strcmp(inputRow , SUB_CODE) == 0) {

            if (strcmp(inputRow , LOG_CODE) == 0)
                ret = clientLoginSubscribeFunction(LOGIN_TYPE) ;
            
            else
                ret = clientLoginSubscribeFunction(SUB_TYPE) ;

        }

        else if (strcmp(inputRow , EXIT_CODE) == 0) {
            clientExitFunction(0) ;
        }

        else {
            ret = NEG_TYPE_0 ;
            printf("Codice Non Valido\n") ;
        }
            
    }

    clientMessageFunction() ;
}


void clientExitFunction(int how) {

    if (how == 0 || (how == SIGHUP && sockDesc != -1)) {
        EXIT_MESSAGE exitMsg ;
        exitMsg.header.type = (int)htonl(EXIT_TYPE) ;
        writeOnSocket((char *)&exitMsg , 0, EXIT_MSG_SIZE) ;
    }
    
    showFormat('*') ;
    printf(" Arrivederci ") ;
    showFormat('*') ;
    printf("\n\n") ;

    if (sockDesc != -1)
        close(sockDesc) ;

    //printf("Premere ENTER per uscire") ;
    fflush(stdout) ;

    exit(0) ;
}


int clientLoginSubscribeFunction(int msgType) {

    if (msgType == LOGIN_TYPE)
        printf("\nLogin\n") ;
    else
        printf("\nIscrizione\n") ;

    
    printf("Inserire Username - Non Superare i %d caratteri e non usare spazi o tab\n" , USER_NAME_MAX_SIZE-2) ;
    printf(">>> ") ;

    char userName[USER_NAME_MAX_SIZE] ;
    do {
        errno = 0 ;
        fgets(userName , USER_NAME_MAX_SIZE , stdin) ;
    } while (errno == EINTR) ;

    if (errno != 0) {
        printf("Errore lettura username\n") ;
        return NEG_TYPE_0 ;
    }

    myStrtok(userName , '\n') ;

    /*
     * Per lo username devo tenere DUE caratteri di avanzo , uno per \n e uno per \0:
     * -    Se dopo la tokenizzazione ho ancora USER_NAME_MAX_SIZE caratteri, significa che ho letto almeno 31 caratteri e 
     *      un newLine, che non è stato messo nel buffer: Quindi lo devo flushare
     * -    Altrimenti ho letto meno di 31 caratteri e quindi posso continuare normalmente
     */
    if (strlen(userName) == USER_NAME_MAX_SIZE-1) {
        freeInputBuff ;
        printf("Username troppo lungo\n") ;
        return NEG_TYPE_0 ;
    }

    int i ;
    for (i = 0 ; i < strlen(userName) ; i++)
        if (userName[i] == ' ' || userName[i] == '\t') {
            printf("Username non deve contenere spazi , tab o new line\n") ;
            return NEG_TYPE_0 ;
        }

    if (strlen(userName) == 0) {
        printf("Username inserito non valido\n") ;
        return NEG_TYPE_0 ;
    }

    
    char passWord[PASSWD_MAX_SIZE] ;
    printf("Inserire Password - Non Superare i %d caratteri e non usere spazi o tab\n", PASSWD_MAX_SIZE - 2) ;
    printf(">>> ") ;
    
    do {
        errno = 0 ;
        fgets(passWord , PASSWD_MAX_SIZE , stdin) ;
    } while (errno == EINTR) ;

    if (errno != 0) {
        printf("Errore lettura password\n") ;
        return NEG_TYPE_0 ;
    }

    myStrtok(passWord , '\n') ;

    if (strlen(passWord) == PASSWD_MAX_SIZE-1) {
        freeInputBuff ;
        printf("Password troppo lunga\n") ;
        return NEG_TYPE_0 ;
    }

    for (i = 0 ; i < strlen(passWord) ; i++) {
        if (passWord[i] == ' ' || passWord[i] == '\t') {
            printf("Password non deve contenere spazi o tab\n") ;
            return NEG_TYPE_0 ;
        }
    }

    if (strlen(passWord) == 0) {
        printf("Password inserita non valida\n") ;
        return NEG_TYPE_0 ;
    }


    LOGIN_MESSAGE logMsg ;
    logMsg.header.type = (int)htonl(msgType) ;

    strcpy(logMsg.userName , userName) ;
    strcpy(logMsg.passwd , passWord) ;


    writeOnSocket((char *)&logMsg , 0, LOG_MSG_SIZE) ;

    int serverAns = waitForAnswer() ;

    if (serverAns == NEG_TYPE_0) 
        printf("Errore Autenticazione Lato Server\n") ;

    else if (serverAns == NEG_TYPE_1) {
        if (msgType == LOGIN_TYPE)
            printf("Password Errata\n") ;
        else
            printf("Esiste Già Utente Con Stesso Nome\n") ;
    }

    else if (serverAns == NEG_TYPE_2) {
        if (msgType == LOGIN_TYPE)
            printf("Non Esiste Utente Con Questo Nome\n") ;
        else
            printf("Raggiunto Il Massimo Numero Di Iscritti\n") ;
    }
    
   
    if (serverAns == POS_TYPE) {
        clientUserName = (char *)malloc(sizeof(char) * USER_NAME_MAX_SIZE) ;
        if (clientUserName == NULL)
            exitProc("Errore malloc\n") ;
        
        strcpy(clientUserName , userName) ;
    }

    return serverAns ;
}



void readFromSocket(char *msgPtr , int totRead , int sizeToRead) {

    int readBytes ;
    do {
        errno = 0 ;
        //printf("Attesa lettura\n") ;
        readBytes = recv(sockDesc , msgPtr + totRead , sizeToRead - totRead, 0) ;

        struct pollfd pollStruct ;
        pollStruct.fd = sockDesc ;
        pollStruct.events = POLLRDHUP ;

        if (poll(&pollStruct , 1 , 0) == -1)
            exitProc("Errore poll\n") ;

        if ((pollStruct.revents & POLLRDHUP) && readBytes == 0) {
            printf("Connessione Al Server Scaduta\n\n") ;
            clientExitFunction(1) ;
        }

        if (errno == EINTR)
            continue ;

        else
            totRead = totRead + readBytes ;

    } while (totRead < sizeToRead) ;

    return ;
}



int waitForAnswer() {
 
    ANSWER_MESSAGE ansMsg ;

    readFromSocket((char *)&ansMsg, 0, ANS_MSG_SIZE) ;

    ansMsg.header.type = (int)ntohl(ansMsg.header.type) ;

    return ansMsg.header.type ;
}



void writeOnSocket(char *msgPtr , int totWrite , int sizeToWrite) {

    int writeBytes ;
    do {
        errno = 0 ;

        writeBytes = send(sockDesc , msgPtr + totWrite , sizeToWrite - totWrite , MSG_NOSIGNAL) ;
        if (errno == ENOTCONN || errno == EPIPE) {
            printf("Connessione Al Server Scaduta\n") ;
            clientExitFunction(1) ;
        }

        if (errno == EINTR)
            continue ;

        else
            totWrite = totWrite + writeBytes ;
            
    } while (totWrite < sizeToWrite) ;

    return ;
}



void myStrtok(char *string , char delimChar) {
    int i ;
    int strSize = strlen(string) ;
    for (i = 0 ; i < strSize ; i++) 
        if (string[i] == delimChar)
            string[i] = '\0' ;

    //printf("%d\n" , (int)strlen(string)) ;
    return ;
}