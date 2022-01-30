#include "./messageTypes.h"
#include "./serverStructs.h" 


#define MSG_HEADER_SIZE sizeof(HEADER) 
#define ANS_MSG_SIZE sizeof(ANSWER_MESSAGE)
#define LOG_MSG_SIZE sizeof(LOGIN_MESSAGE)
#define EXIT_MSG_SIZE sizeof(EXIT_MESSAGE)
#define SEND_MSG_SIZE sizeof(SEND_MESSAGE)
#define DEL_MSG_SIZE sizeof(DEL_MESSAGE)
#define READ_MSG_SIZE sizeof(READ_MESSAGE)

#define ANOMALY 1

#define FILE_HEADER_SIZE sizeof(CLIENT_FILE_HEADER)

#define FORMAT_SIZE 20
#define BUFF_SIZE 128

#define CLIENTS_INFO_FILE_NAME "./serverDirectory/clientsInfoFile"

#define MAX_CLIENT_NUM 64


 

//In serverMain
void exitProc(char *exitMessage) ;
void retryProc(char *exitMessage) ;
void showFormat(char formatChar) ;
void fillClientList() ;
void treeInsertNode(CLIENT_NODE *newNode) ;
void serverSignalSet() ;
void *controlThreadFunction(void *param) ;
void sigint_handler() ;
void takeAllMutexes(CLIENT_NODE *treeRoot) ;



//serverLog
void *serverStartFunction(void *param) ;
void readFromSocket(char *msgPtr , int totRead , int sizeToRead) ;
void writeOnSocket(char *msgPtr , int totWrite , int sizeToWrite) ;
int serverLoginFunction(LOGIN_MESSAGE *logMsgPtr) ;
int serverSubscribeFunction(LOGIN_MESSAGE *logMsgPtr) ;
void serverExitFunction() ;
void answerToClient(int msgType) ;
void clientFileInit(int fileDesc, int how) ;



//serverMessage
void serverMessageFunction(LOGIN_MESSAGE *logMsgPtr) ;
int serverSendFunction(HEADER *msgHeaderPtr) ;
int serverReadFunction(HEADER *msgHeaderPtr , CLIENT_NODE *clientNode) ;
int serverDeleteFunction(HEADER *msgHeaderPtr , CLIENT_NODE *clientNode) ;

CLIENT_NODE *treeSearchNode(char *userName) ;