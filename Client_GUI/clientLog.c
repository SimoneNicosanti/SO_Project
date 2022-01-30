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
#include <gtk/gtk.h>

#include "clientDeclaration.h"


#define SERVER_IP_ADDR "127.0.1.1"
#define SERVER_PORT_NUM 49152


extern int sockDesc ;
int serverAns = NEG_TYPE_0 ;

char *clientUserName ;

GtkBuilder *myBuilder ;
GtkWidget *loginWindow ;


void clientExitFunction(int how) {

    if (how == 0) {
        EXIT_MESSAGE exitMsg ;
        exitMsg.header.type = (int)htonl(EXIT_TYPE) ;
        writeOnSocket((char *)&exitMsg , 0, EXIT_MSG_SIZE) ;
    }
    
    close(sockDesc) ;
    exit(0) ;
}



void on_loginWindow_destroy() {
    clientExitFunction(0) ;
    gtk_main_quit() ;
    exit(0) ;
    return ;
}



void clientStartFunction() {

    gtk_init(NULL , NULL) ;

    myBuilder = gtk_builder_new() ;
    #ifndef RES
    gtk_builder_add_from_file(myBuilder , "clientGUI.glade", NULL) ;
    #else
    gtk_builder_add_from_resource(myBuilder , "/clientGUI/clientGUI.glade" , NULL) ;
    #endif
    
    gtk_builder_connect_signals(myBuilder , NULL) ;

    loginWindow = GTK_WIDGET(gtk_builder_get_object(myBuilder , "loginWindow")) ;

    gtk_entry_set_max_length(GTK_ENTRY(gtk_builder_get_object(myBuilder, "usernameEntry")) , USER_NAME_MAX_SIZE-1) ;
    gtk_entry_set_max_length(GTK_ENTRY(gtk_builder_get_object(myBuilder , "passwordEntry")) , PASSWD_MAX_SIZE-1) ;
    
    gtk_widget_show(loginWindow) ;

    gtk_main() ;

    if (serverAns == POS_TYPE)
        clientMessageFunction() ;

    clientExitFunction(0) ;
}



void on_loginButton_clicked() {

    GtkEntry *usernameEntry = GTK_ENTRY(gtk_builder_get_object(myBuilder, "usernameEntry")) ;
    GtkEntry *passwordEntry = GTK_ENTRY(gtk_builder_get_object(myBuilder , "passwordEntry")) ;

    const char *username = gtk_entry_get_text(usernameEntry) ;
    const char *password = gtk_entry_get_text(passwordEntry) ;

    serverAns = clientLoginSubscribeFunction(username , password , LOGIN_TYPE) ;

    if (serverAns == POS_TYPE) {
        gtk_widget_hide(loginWindow) ;
        gtk_main_quit() ;
    }

    return ;
}



void on_subscribeButton_clicked() {

    GtkEntry *usernameEntry = GTK_ENTRY(gtk_builder_get_object(myBuilder, "usernameEntry")) ;
    GtkEntry *passwordEntry = GTK_ENTRY(gtk_builder_get_object(myBuilder , "passwordEntry")) ;

    const char *username = gtk_entry_get_text(usernameEntry) ;
    const char *password = gtk_entry_get_text(passwordEntry) ;

    serverAns = clientLoginSubscribeFunction(username , password , SUB_TYPE) ;

    if (serverAns == POS_TYPE) {
        gtk_widget_hide(loginWindow) ;
        gtk_main_quit() ;
    }

    return ;
}



int clientLoginSubscribeFunction(const char *username , const char *password , int msgType) {

    GtkLabel *loginLabel= GTK_LABEL(gtk_builder_get_object(myBuilder, "esitoLoginLabel")) ;
    int i ;

    for (i = 0 ; i < strlen(username) ; i++)
        if (username[i] == ' ' || username[i] == '\t') {
            gtk_label_set_text(loginLabel , "Username: Caratteri non Validi") ;
            return NEG_TYPE_0 ;
        }

    if (strlen(username) == 0) {
        gtk_label_set_text(loginLabel , "Username: Lunghezza Non Valida") ;
        return NEG_TYPE_0 ;
    }


    for (i = 0 ; i < strlen(password) ; i++) {
        if (password[i] == ' ' || password[i] == '\t') {
            gtk_label_set_text(loginLabel , "Password: Caratteri non Validi") ;
            return NEG_TYPE_0 ;
        }
    }

    if (strlen(password) == 0) {
        gtk_label_set_text(loginLabel , "Password: Lunghezza non Valida") ;
        return NEG_TYPE_0 ;
    }


    LOGIN_MESSAGE logMsg ;
    logMsg.header.type = (int)htonl(msgType) ;

    strcpy(logMsg.userName , username) ;
    strcpy(logMsg.passwd , password) ;

    writeOnSocket((char *)&logMsg , 0, LOG_MSG_SIZE) ;

    int serverAns = waitForAnswer() ;

    if (serverAns == POS_TYPE) {
        gtk_label_set_text(loginLabel , "Accesso Consentito") ;
        clientUserName = (char *)malloc(sizeof(char) * strlen(username)) ;
        if (clientUserName == NULL)
            exitProc("Errore malloc\n") ;
        strcpy(clientUserName , username) ;
    }

    else if (serverAns == NEG_TYPE_0) 
        gtk_label_set_text(loginLabel , "Errore Lato Server") ;

    else if (serverAns == NEG_TYPE_1) {
        if (msgType == LOGIN_TYPE)
            gtk_label_set_text(loginLabel , "Password: Errata") ;
        else
            gtk_label_set_text(loginLabel , "Username: Esiste Già") ;
    }

    else if (serverAns == NEG_TYPE_2) {
        if (msgType == LOGIN_TYPE)
            gtk_label_set_text(loginLabel , "Username: Nome Non Esiste") ;
        else
            gtk_label_set_text(loginLabel , "Massimo Numero di Clients Raggiunto") ;
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

