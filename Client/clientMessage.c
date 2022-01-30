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
#include <sys/wait.h>



#include "clientDeclaration.h"


#define SERVER_IP_ADDR "127.0.1.1"
#define SERVER_PORT_NUM 49152


extern int sockDesc ;
extern char *clientUserName ;



void clientMessageFunction() {

    int ret ;
    char inputRow[BUFF_SIZE] ;
    
    do {

        printf("\nInserire Codice Opzione\n") ;
        printf("%s. Leggi Tutti\n" , READ_ALL_CODE) ;
        printf("%s. Leggi Per Mittente\n", READ_SOURCE_CODE) ;
        printf("%s. Leggi Per Oggetto\n" , READ_OBJ_CODE) ;
        printf("%s. Invia Messaggio\n" , SEND_CODE) ;
        printf("%s. Cancella Messaggio\n", DEL_CODE) ;
        printf("%s. Uscire\n" , EXIT_CODE) ;
        printf(">>> ") ;

        do {
            errno = 0 ;
            fgets(inputRow , BUFF_SIZE , stdin) ;
        } while (errno == EINTR) ;

        if (errno != 0) {
            inputRow[0] = '\0' ;
            continue ;
        }

        myStrtok(inputRow , '\n') ;

        if (strlen(inputRow) == BUFF_SIZE-1) {
            freeInputBuff ;
            inputRow[0] = '\0' ;
        }


        if (strcmp(inputRow , READ_ALL_CODE) == 0 || strcmp(inputRow , READ_SOURCE_CODE) == 0 || strcmp(inputRow , READ_OBJ_CODE) == 0) 
            clientReadFunction(inputRow) ;

        else if (strcmp(inputRow , SEND_CODE) == 0)
            clientSendFunction() ;

        else if (strcmp(inputRow , DEL_CODE) == 0)
            clientDeleteFunction() ;

        else if (strcmp(inputRow , EXIT_CODE) != 0) 
            printf("Opzione Non Valida\n") ;

    } while (strcmp(inputRow , EXIT_CODE) != 0) ;

    clientExitFunction(0) ;
}



void clientSendFunction() {

    printf("\nInvio Messaggio\n") ;

    SEND_MESSAGE newMessage ;
    newMessage.header.type = (int)htonl(SEND_TYPE) ;


    char destUser[USER_NAME_MAX_SIZE] ;
    printf("Inserire Destinatario\n") ;
    printf(">>> ") ;
    do {
        errno = 0 ;
        fgets(destUser , USER_NAME_MAX_SIZE , stdin) ;
    } while (errno == EINTR) ;

    if (errno != 0) {
        printf("Errore lettura destinatatio\n") ;
        return ;
    }

    myStrtok(destUser , '\n') ;

    if (strlen(destUser) == USER_NAME_MAX_SIZE-1) {
        printf("Nome Destinatario Troppo Lungo: non superare i %d caratteri\n", USER_NAME_MAX_SIZE-2) ;
        freeInputBuff ;
        return ;
    }

    if (strlen(destUser) == 0) {
        printf("Nessun Destinatario Inserito\n") ;
        return ;
    }

    int i ;
    for (i = 0 ; i < strlen(destUser) ; i++) {
        if (destUser[i] == ' ' || destUser[i] == '\t') {
            printf("Nome Destinatario Contiene Caratteri Non Validi\n") ;
            return ;
        }
    }


    char object[OBJECT_MAX_SIZE] ;
    printf("Inserire Oggetto\n") ;
    printf(">>> ") ;
    do {
        errno = 0 ;
        fgets(object , OBJECT_MAX_SIZE , stdin) ;
    } while (errno == EINTR) ;

    if (errno != 0) {
        printf("Errore lettura oggetto\n") ;
        return ;
    }

    myStrtok(object, '\n') ;

    if (strlen(object) ==  OBJECT_MAX_SIZE-1) {
        printf("Oggetto Inserito Troppo Lungo: Non Superare i %d caratteri\n", OBJECT_MAX_SIZE-2) ;
        freeInputBuff ;
        return ;
    }


    char payload[MESSAGE_MAX_SIZE] ;
    printf("Inserire Messaggio\n") ;
    printf(">>> ") ;
    do {
        errno = 0 ;
        fgets(payload , MESSAGE_MAX_SIZE , stdin) ;
    } while (errno == EINTR) ;

    if (errno != 0) {
        printf("Errore lettura messaggio\n") ;
        return ;
    }

    myStrtok(payload , '\n') ;

    if (strlen(payload) == MESSAGE_MAX_SIZE-1) {
        printf("Messaggio troppo lungo\n") ;
        freeInputBuff ;
        return ;
    }

    
    //printf("%s\n" , clientUserName) ;
    strcpy(newMessage.sendUser , clientUserName) ;
    strcpy(newMessage.destUser , destUser) ;
    strcpy(newMessage.object , object) ;
    strcpy(newMessage.payload , payload) ;

    
    writeOnSocket((char *)&newMessage , 0, SEND_MSG_SIZE) ;

    int serverAns = waitForAnswer() ;

    if (serverAns == NEG_TYPE_0)
        printf("Errore Invio Lato Server\n") ;
    else if (serverAns == NEG_TYPE_1)
        printf("Non Esiste Utente Destinatario\n") ;
    else if (serverAns == NEG_TYPE_2)
        printf("Spazio Insufficiente Per Destinatario\n") ;
    else
        printf("Messaggio Inviato Con Successo\n") ;

    return ;
}



void clientReadFunction(char *inputRow) {

    printf("\nLettura Messaggi\n") ;

    READ_MESSAGE readMsg ;
    memset(&readMsg , 0, READ_MSG_SIZE) ;

    if (strcmp(inputRow , READ_SOURCE_CODE) == 0 || strcmp(inputRow , READ_OBJ_CODE) == 0) {
        printf("Inserire Campo Ricerca\n") ;
        printf(">>> ") ;

        int searchSize = (USER_NAME_MAX_SIZE > OBJECT_MAX_SIZE) ?  USER_NAME_MAX_SIZE : OBJECT_MAX_SIZE ;
        char searchInfo[searchSize] ;
        do {
            errno = 0 ;
            fgets(searchInfo , searchSize , stdin) ;
        } while (errno == EINTR) ;

        if (errno != 0) {
            printf("Errore lettura campo\n") ;
            return ;
        }

        myStrtok(searchInfo , '\n') ;

        if (strcmp(inputRow , READ_OBJ_CODE) == 0) {
            if (strlen(searchInfo) == OBJECT_MAX_SIZE-1) {
                printf("Oggetto Inserito Troppo Lungo: Non superare i %d caratteri\n", OBJECT_MAX_SIZE-2) ;
                freeInputBuff ;
                return ;
            }
            strcpy(readMsg.object , searchInfo) ;
        }

        if (strcmp(inputRow , READ_SOURCE_CODE) == 0) {
            if (strlen(searchInfo) == USER_NAME_MAX_SIZE-1) {
                printf("Nome Utente Inserito Troppo Lungo: Non superare i %d caratteri\n", USER_NAME_MAX_SIZE-2) ;
                freeInputBuff ;
                return ;
            }

            if (strlen(searchInfo) == 0) {
                printf("Nessun Mittente Inserito\n") ;
                return ;
            }

            strcpy(readMsg.sourceUser , searchInfo) ;
        }
    }

    else {
        readMsg.sourceUser[0] = '\0' ;
        readMsg.object[0] = '\0' ;
    }

    readMsg.header.type = (int)htonl(READ_TYPE) ;
    


    writeOnSocket((char *)&readMsg , 0, READ_MSG_SIZE) ;

    
    showFormat('#') ;
    printf("\tMessaggi Ricevuti\t") ;
    showFormat('#') ;
    printf("\n\n") ;

    HEADER rcvHeader ;

    readFromSocket((char *)&rcvHeader , 0, MSG_HEADER_SIZE) ;
    rcvHeader.type = (int)ntohl(rcvHeader.type) ;
    rcvHeader.msgNum = (int)ntohl(rcvHeader.msgNum) ;


    while (rcvHeader.type == SEND_TYPE) {

        SEND_MESSAGE rcvMessage ;
        rcvMessage.header.type = rcvHeader.type ;
        rcvMessage.header.msgNum = rcvHeader.msgNum ;

        readFromSocket((char *)&rcvMessage , MSG_HEADER_SIZE , SEND_MSG_SIZE) ;

        printf("Mittente : %s\n" , rcvMessage.sendUser) ;
        printf("Oggetto : %s\n" , rcvMessage.object) ;
        printf("Codice Messaggio : %d\n" , rcvMessage.header.msgNum) ;
        printf(">>> %s", rcvMessage.payload) ;
        printf(" <<<\n\n") ;

        readFromSocket((char *)&rcvHeader , 0, MSG_HEADER_SIZE) ;
        rcvHeader.type = (int)ntohl(rcvHeader.type) ;
        rcvHeader.msgNum = (int)ntohl(rcvHeader.msgNum) ;

    }

    showFormat('#') ;
    printf("\t  Fine Messaggi\t\t") ;
    showFormat('#') ;
    printf("\n") ;


    return ;
}



void clientDeleteFunction() {

    printf("\nCancellazione Messaggio\n") ;

    DEL_MESSAGE delMsg ;


    printf("Inserire Codice Messaggio Da Cancellare\n") ;
    printf(">>> ") ;

    char inputRow[BUFF_SIZE] ;
    do {
        errno = 0 ;
        fgets(inputRow , BUFF_SIZE , stdin) ;
    } while (errno == EINTR) ;

    if (errno != 0) {
        printf("Errore lettura codice\n") ;
        return ;
    }

    myStrtok(inputRow , '\n') ;

    if (strlen(inputRow) == BUFF_SIZE-1) {
        printf("Numero Inserito Non Valido\n") ;
        freeInputBuff ;
        return ;
    }

    int i ;
    for (i = 0 ; i < strlen(inputRow) ; i++) {
        if (inputRow[i] > 57 || inputRow[i] < 48) {
            printf("Numero inserito non valido\n") ;
            return ;
        }
    }

    int msgNum = strtol(inputRow , NULL, 10) ;
    if (strlen(inputRow) == 0) {
        printf("Numero Inserito Non Valido\n") ;
        return ;
    }

    /*
    if (msgNum < 0) {
        printf("Numero Inserito Non Valido: Deve Essere Maggiore o Uguale a Zero\n") ;
        return ;
    }
    */


    delMsg.header.msgNum = (int)htonl(msgNum) ;
    delMsg.header.type = (int)htonl(DEL_TYPE) ;

    writeOnSocket((char *)&delMsg , 0, DEL_MSG_SIZE) ;


    int serverAns = waitForAnswer() ;

    if (serverAns == POS_TYPE)
        printf("Messaggio Cancellato Con Successo\n") ;
    else
        printf("Errore Cancellazione Messaggio\n") ;

    return ;
}


