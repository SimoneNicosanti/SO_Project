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
#include <gtk/gtk.h>

#include "clientDeclaration.h"


#define SERVER_IP_ADDR "127.0.1.1"
#define SERVER_PORT_NUM 49152


extern int sockDesc ;
extern char *clientUserName ;

extern GtkBuilder *myBuilder ;

GtkWidget *mainWindow ;
GtkLabel *esitoLabel ;

GtkWidget *readMessagesWindow ;
GtkTextView *readMessagesTextView ;
GtkTextBuffer *readMessagesTextBuffer ;

GtkWidget *searchMessageWindow ;

GtkWidget *sendMessageWindow ;

GtkWidget *deleteMessageWindow ;

char *searchCode ;


void clientMessageFunction() {

    //printf("socket %d" ,sockDesc) ;

    mainWindow = GTK_WIDGET(gtk_builder_get_object(myBuilder , "mainProgramWindow")) ;
    esitoLabel = GTK_LABEL(gtk_builder_get_object(myBuilder , "esitoOperazioneLabel")) ;

    readMessagesWindow = GTK_WIDGET(gtk_builder_get_object(myBuilder , "readMessagesWindow")) ;
    readMessagesTextView = GTK_TEXT_VIEW(gtk_builder_get_object(myBuilder , "readMessagesTextView")) ;
    readMessagesTextBuffer = GTK_TEXT_BUFFER(gtk_builder_get_object(myBuilder , "readMessagesTextBuffer")) ;

    searchMessageWindow = GTK_WIDGET(gtk_builder_get_object(myBuilder , "searchMessageWindow")) ;

    sendMessageWindow = GTK_WIDGET(gtk_builder_get_object(myBuilder , "sendMessageWindow")) ;

    deleteMessageWindow = GTK_WIDGET(gtk_builder_get_object(myBuilder , "deleteMessageWindow")) ;

    
    gtk_widget_show_all(mainWindow) ;

    gtk_main() ;

    clientExitFunction(0) ;
}



void clientReadFunction(char *inputRow) {

    READ_MESSAGE readMsg ;
    memset(&readMsg , 0, READ_MSG_SIZE) ;

    
    if (strcmp(inputRow , READ_SOURCE_CODE) == 0 || strcmp(inputRow , READ_OBJ_CODE) == 0) {
        
        GtkEntry *searchEntry = GTK_ENTRY(gtk_builder_get_object(myBuilder, "searchEntryInfo")) ;

        const char *searchInfo = gtk_entry_get_text(searchEntry) ;


        if (strcmp(inputRow , READ_OBJ_CODE) == 0) 
            strcpy(readMsg.objectUser , searchInfo) ;


        if (strcmp(inputRow , READ_SOURCE_CODE) == 0) {

            if (strlen(searchInfo) == 0) {
                gtk_label_set_text(esitoLabel , "Negativo : Nessun Utente Inserito\n") ;
                return ;
            }

            int i ;
            for (i = 0 ; i < strlen(searchInfo) ; i++) {
                if (searchInfo[i] == ' ' || searchInfo[i] == '\t') {
                    gtk_label_set_text(esitoLabel , "Negativo: Caratteri non validi") ;
                    return ;
                }
            }

            strcpy(readMsg.sourceUser , searchInfo) ;
        }
    }
    
    readMsg.header.type = (int)htonl(READ_TYPE) ;

    writeOnSocket((char *)&readMsg , 0, READ_MSG_SIZE) ;


    FILE *readFilePtr =  fopen(READ_MESSAGE_FILE , "w+") ;
    if (readFilePtr == NULL)
        exitProc("Errore apertura stream\n") ;
    
    //showFormat('+') ;

    HEADER rcvHeader ;


    readFromSocket((char *)&rcvHeader , 0, MSG_HEADER_SIZE) ;
    rcvHeader.type = (int)ntohl(rcvHeader.type) ;
    rcvHeader.msgNum = (int)ntohl(rcvHeader.msgNum) ;

    while (rcvHeader.type == SEND_TYPE) {

        SEND_MESSAGE rcvMessage ;
        rcvMessage.header.type = rcvHeader.type ;
        rcvMessage.header.msgNum = rcvHeader.msgNum ;

        readFromSocket((char *)&rcvMessage , MSG_HEADER_SIZE , SEND_MSG_SIZE) ;

        fprintf(readFilePtr , "Mittente : %s\n" , rcvMessage.sendUser) ;
        fprintf(readFilePtr , "Oggetto : %s\n" , rcvMessage.object) ;
        fprintf(readFilePtr , "Numero Messaggio : %d\n" , rcvMessage.header.msgNum) ;
        fprintf(readFilePtr , ">>> %s", rcvMessage.payload) ;
        fprintf(readFilePtr , " <<<\n\n") ;

        readFromSocket((char *)&rcvHeader , 0, MSG_HEADER_SIZE) ;
        rcvHeader.type = (int)ntohl(rcvHeader.type) ;
        rcvHeader.msgNum = (int)ntohl(rcvHeader.msgNum) ;

    }

    fflush(readFilePtr) ;
    fclose(readFilePtr) ;

    int readFileDesc = open(READ_MESSAGE_FILE , O_RDONLY) ;
    int fileSize = lseek(readFileDesc , 0, SEEK_END) ;

    char *fileBuff = (char *)malloc(sizeof(char) * (fileSize+1)) ;
    if (fileBuff == NULL)
        exitProc("Errore malloc\n") ;

    lseek(readFileDesc , 0, SEEK_SET) ;

    read(readFileDesc , fileBuff , fileSize) ;
    fileBuff[fileSize] = '\0' ;

    close(readFileDesc) ;
    unlink(READ_MESSAGE_FILE) ;

    /*
    readMessagesWindow = GTK_WIDGET(gtk_builder_get_object(myBuilder , "readMessagesWindow")) ;
    readMessagesTextView = GTK_TEXT_VIEW(gtk_builder_get_object(myBuilder , "readMessagesTextView")) ;
    readMessagesTextBuffer = GTK_TEXT_BUFFER(gtk_builder_get_object(myBuilder , "readMessagesTextBuffer")) ;
    */
    gtk_text_buffer_set_text(readMessagesTextBuffer , fileBuff , -1) ;
    gtk_text_view_set_monospace(readMessagesTextView , 1) ;

    free(fileBuff) ;

    gtk_widget_show_now(readMessagesWindow) ;

    gtk_label_set_text(esitoLabel , "Positivo") ;
    
    return ;
}



void clientSendFunction() {

    SEND_MESSAGE newMessage ;
    newMessage.header.type = (int)htonl(SEND_TYPE) ;


    GtkEntry *entryDestinatario = GTK_ENTRY(gtk_builder_get_object(myBuilder , "entryDestinatario")) ;
    GtkEntry *entryOggetto = GTK_ENTRY(gtk_builder_get_object(myBuilder , "entryOggetto")) ;

    const char *destUser = gtk_entry_get_text(entryDestinatario) ;

    if (strlen(destUser) == 0) {
        gtk_label_set_text(esitoLabel , "Negativo. Username non inserito") ;
        return ;
    }

    int i ;
    for (i = 0 ; i < strlen(destUser) ; i++) {
        if (destUser[i] == ' ' || destUser[i] == '\t') {
            gtk_label_set_text(esitoLabel , "Negativo. Caratteri non validi") ;
            return ;
        }
    }


    const char *object = gtk_entry_get_text(entryOggetto) ;

    GtkTextIter start , end ;
    GtkTextView *textViewPayload = GTK_TEXT_VIEW(gtk_builder_get_object(myBuilder , "textViewPayload")) ;

    GtkTextBuffer *payloadBuffer = gtk_text_view_get_buffer(textViewPayload) ;
    
    gtk_text_buffer_get_bounds(payloadBuffer , &start , &end) ;
    
    char *payload = gtk_text_buffer_get_text(payloadBuffer , &start , &end , FALSE) ;

    
    if (strlen(payload) >= MESSAGE_MAX_SIZE) {
        gtk_label_set_text(esitoLabel , "Negativo. Messaggio Troppo Lungo") ;
        free(payload) ;
        return ;
    }

    
    //printf("%s\n" , clientUserName) ;
    strcpy(newMessage.sendUser , clientUserName) ;
    strcpy(newMessage.destUser , destUser) ;
    strcpy(newMessage.object , object) ;
    strcpy(newMessage.payload , payload) ;

    free(payload) ;
    
    writeOnSocket((char *)&newMessage , 0, SEND_MSG_SIZE) ;


    int serverAns = waitForAnswer() ;


    if (serverAns == NEG_TYPE_0)
        gtk_label_set_text(esitoLabel , "Negativo. Errore Invio Lato Server") ;
    else if (serverAns == NEG_TYPE_1)
        gtk_label_set_text(esitoLabel , "Negativo. Destinatario Non Esiste") ;
    else if (serverAns == NEG_TYPE_2)
        gtk_label_set_text(esitoLabel , "Negativo. Spazio Insufficiente Per Destinatario") ;
    else
        gtk_label_set_text(esitoLabel , "Positivo. Messaggio Inviato Con Successo") ;

    return ;
}



void clientDeleteFunction() {

    DEL_MESSAGE delMsg ;

    GtkSpinButton *deleteSpinButton = GTK_SPIN_BUTTON(gtk_builder_get_object(myBuilder , "deleteSpinButton")) ;

    int msgNum = gtk_spin_button_get_value(deleteSpinButton) ;

    if (msgNum < 0) {
        gtk_label_set_text(esitoLabel , "Negativo. Numero Non Valido") ;
        return ;
    }


    delMsg.header.msgNum = (int)htonl(msgNum) ;
    delMsg.header.type = (int)htonl(DEL_TYPE) ;

    writeOnSocket((char *)&delMsg , 0, DEL_MSG_SIZE) ;


    int serverAns = waitForAnswer() ;

    if (serverAns == POS_TYPE)
        gtk_label_set_text(esitoLabel , "Positivo. Cancellato con Successo") ;
    else
        gtk_label_set_text(esitoLabel , "Negativo. Impossibile Cancellare") ;

    return ;
}



void on_mainProgramWindow_destroy() {
    clientExitFunction(0) ;
    gtk_main_quit() ;
    exit(0) ;
    return ;
}



void on_readAllButton_clicked() {

    clientReadFunction(READ_ALL_CODE) ;
    return ;
}


void on_readSourceButton_clicked() {

    GtkLabel *searchLabel = GTK_LABEL(gtk_builder_get_object(myBuilder , "searchLabel")) ;
    gtk_label_set_text(searchLabel , "Ricerca Per Mittente") ;
    searchCode = READ_SOURCE_CODE ;

    GtkEntry *searchEntry = GTK_ENTRY(gtk_builder_get_object(myBuilder, "searchEntryInfo")) ;
    gtk_entry_set_max_length(searchEntry , USER_NAME_MAX_SIZE-1) ;
        
    gtk_widget_show_now(searchMessageWindow) ;
    gtk_widget_hide(mainWindow) ;

    return ;
}


void on_readObjectButton_clicked() {

    GtkLabel *searchLabel = GTK_LABEL(gtk_builder_get_object(myBuilder , "searchLabel")) ;
    gtk_label_set_text(searchLabel , "Ricerca Per Oggetto") ;
    searchCode = READ_OBJ_CODE ;

    GtkEntry *searchEntry = GTK_ENTRY(gtk_builder_get_object(myBuilder, "searchEntryInfo")) ;
    gtk_entry_set_max_length(searchEntry , OBJECT_MAX_SIZE) ;

    gtk_widget_show_now(searchMessageWindow) ;
    gtk_widget_hide(mainWindow) ;
    
    return ;
}


void on_searchEnterButton_clicked() {
    gtk_widget_hide(searchMessageWindow) ;

    gtk_widget_show_all(mainWindow) ;

    clientReadFunction(searchCode) ;
}



void on_sendMessageButton_clicked() {
    
    GtkTextBuffer *payloadBuffer = GTK_TEXT_BUFFER(gtk_builder_get_object(myBuilder, "payloadBuffer")) ;
    gtk_text_buffer_set_text(payloadBuffer , "", 0) ;

    gtk_widget_hide(mainWindow) ;
    gtk_widget_show(sendMessageWindow) ;

    GtkEntry *entryDestinatario = GTK_ENTRY(gtk_builder_get_object(myBuilder , "entryDestinatario")) ;
    GtkEntry *entryOggetto = GTK_ENTRY(gtk_builder_get_object(myBuilder , "entryOggetto")) ;

    gtk_entry_set_max_length(entryDestinatario , USER_NAME_MAX_SIZE-1) ;
    gtk_entry_set_max_length(entryOggetto , OBJECT_MAX_SIZE-1) ;

    return ;
}


void on_confirmSendButton_clicked() {
    clientSendFunction() ;

    gtk_widget_hide(sendMessageWindow) ;
    gtk_widget_show(mainWindow) ;

    return ;
}



void on_deleteMessageButton_clicked() {

    GtkAdjustment *adjustment = GTK_ADJUSTMENT(gtk_builder_get_object(myBuilder , "spinButtonAdj")) ;

    gtk_adjustment_set_upper(adjustment , MAX_MSG_NUM) ;
    gtk_adjustment_set_lower(adjustment, -1) ;

    gtk_widget_show(deleteMessageWindow) ;
    gtk_widget_hide(mainWindow) ;

    return ;
}


void on_confirmDeleteButton_clicked() {

    gtk_widget_hide(deleteMessageWindow) ;
    
    clientDeleteFunction() ;

    gtk_widget_show(mainWindow) ;

    return ;
}



gint on_readMessagesWindow_delete_event(){
    gtk_widget_hide(readMessagesWindow) ;
    return TRUE ;
}

gint on_searchMessageWindow_delete_event() {
    gtk_widget_hide(searchMessageWindow) ;
    gtk_widget_show(mainWindow) ;
    return TRUE ;
}

gint on_sendMessageWindow_delete_event() {
    gtk_widget_hide(sendMessageWindow) ;
    gtk_widget_show(mainWindow) ;
    return TRUE ;
}

gint on_deleteMessageWindow_delete_event() {

    gtk_widget_show(mainWindow) ;
    gtk_widget_hide(deleteMessageWindow) ;

    return TRUE ;
}
