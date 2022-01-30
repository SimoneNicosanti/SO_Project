#include "messageTypes.h"

#define MSG_HEADER_SIZE sizeof(HEADER) 
#define ANS_MSG_SIZE sizeof(ANSWER_MESSAGE)
#define LOG_MSG_SIZE sizeof(LOGIN_MESSAGE)
#define EXIT_MSG_SIZE sizeof(EXIT_MESSAGE)
#define SEND_MSG_SIZE sizeof(SEND_MESSAGE)
#define DEL_MSG_SIZE sizeof(DEL_MESSAGE)
#define READ_MSG_SIZE sizeof(READ_MESSAGE)


#define FORMAT_SIZE 20
#define BUFF_SIZE 32
#define READ_MESSAGE_FILE ".readMessageFile"

#define freeInputBuff while (getchar() != '\n') ;


 
#define EXIT_CODE "z"
#define LOG_CODE "a"
#define SUB_CODE "b"

#define READ_ALL_CODE "a"
#define READ_SOURCE_CODE "b"
#define READ_OBJ_CODE "c"
#define SEND_CODE "d"
#define DEL_CODE "e"



// In clientMain
void exitProc(char *exitMessage) ;
void retryProc(char *exitMessage) ;
void showFormat(char formatChar) ;
void clientSignalSet() ;


// In clientLog
void clientStartFunction() ;
void readFromSocket(char *msgPtr , int totRead , int sizeToRead) ;
void writeOnSocket(char *msgPtr , int totWrite , int sizeToWrite) ;



int clientLoginSubscribeFunction(const char *username , const char *password , int msgType) ;
int waitForAnswer() ;
void clientExitFunction(int how) ;
void myStrtok(char *string , char delim) ;


// In clientMessage
void clientMessageFunction() ;
void clientSendFunction() ;
void clientReadFunction(char *inputRow) ;
void clientDeleteFunction() ;



//Gestione GUI
void on_loginButton_clicked() ;
void on_subscribeButton_cliecked() ;
void on_loginWindow_destroy() ;

void on_mainProgramWindow_destroy() ;
void on_readAllButton_clicked() ;
void on_readObjectButton_clicked() ;
void on_readSourceButton_clicked() ;
void on_searchEnterButton_clicked() ;
void on_confirmSendButton_clicked() ;
void on_sendMessageButton_clicked() ;
void on_deleteMessageButton_clicked() ;
void on_confirmDeleteButton_clicked() ;


