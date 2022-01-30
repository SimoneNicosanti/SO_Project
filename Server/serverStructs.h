#define MAX_MSG_NUM 512

typedef struct _CLIENT_NODE {
    pthread_mutex_t clientMutex ;
    char userName[USER_NAME_MAX_SIZE] ;
    struct _CLIENT_NODE *leftChild ;
    struct _CLIENT_NODE *rightChild ;
} CLIENT_NODE ;



typedef struct _CLIENT_FILE_HEADER {
    /* 
     * 0 --> Posizione Occupta
     * 1 --> Posizione Libera
     */
    int freePos[MAX_MSG_NUM] ;
    int startMessagePos[MAX_MSG_NUM] ;
    int totMsg ;
} CLIENT_FILE_HEADER ;