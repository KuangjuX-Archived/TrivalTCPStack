#include <pthread.h>

typedef struct list_head {

} list_head;

typedef struct rbroot {

} rbroot;

typedef struct epitem {
    
} epitem;

typedef struct eventepoll {
    pthread_mutex_t lock;

    list_head rdllist;
} eventepoll;