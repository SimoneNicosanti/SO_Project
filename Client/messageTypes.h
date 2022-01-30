#define USER_NAME_MAX_SIZE 32
#define OBJECT_MAX_SIZE 32
#define PASSWD_MAX_SIZE 16
#define ANS_TEXT_MAX_SIZE 32
#define MESSAGE_MAX_SIZE 256
 
#define LOGIN_TYPE 1
#define SUB_TYPE 0
#define EXIT_TYPE 2
#define SEND_TYPE 4
#define READ_TYPE 3
#define DEL_TYPE 5

#define POS_TYPE 6
#define NEG_TYPE_0 -1   // Errore lato server
#define NEG_TYPE_1 -2
#define NEG_TYPE_2 -3



/*
 * Campo tipo dei messaggi :
 * - Tipo -1 : Risposta negativa
 * - Tipo 0 : Iscrizione
 * - Tipo 1 : Login
 * - Tipo 2 : Uscita
 * - Tipo 3 : Lettura tutti messaggi
 * - Tipo 4 : Spedizione nuovo messaggio
 * - Tipo 5 : Cancellazione messaggio
 * - Tipo 6 : Risposta Positiva
 */


typedef struct _header {
    int type ;
    int msgNum ;
} HEADER ;



typedef struct _ANSWER_MESSAGE {
    HEADER header ;
} ANSWER_MESSAGE ;



typedef struct _loginMessage {
    HEADER header ;
    char userName[USER_NAME_MAX_SIZE] ;
    char passwd[PASSWD_MAX_SIZE] ;
} LOGIN_MESSAGE ;



typedef struct _exitMessage {
    HEADER header ;
} EXIT_MESSAGE ;



typedef struct _sendMessage {
    HEADER header ;
    char destUser[USER_NAME_MAX_SIZE] ;
    char sendUser[USER_NAME_MAX_SIZE] ;
    char object[USER_NAME_MAX_SIZE] ;

    char payload[MESSAGE_MAX_SIZE] ;
} SEND_MESSAGE ;



typedef struct _deleteMessage { 
    HEADER header ;
} DEL_MESSAGE ;


typedef struct _readMessage {
    HEADER header ;
    char sourceUser[USER_NAME_MAX_SIZE] ;
    char object[OBJECT_MAX_SIZE] ;
} READ_MESSAGE ;


