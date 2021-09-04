#include <pthread.h>



// Double list in linux
typedef struct list_head {
    struct list_head* prev;
    struct list_head* next;
    void* data;
} list_head;

typedef struct wait_queue_head_t {
    pthread_mutex_t lock;
    list_head task_list;
} wait_queue_head_t;

typedef struct rbroot {

} rbroot;

typedef struct epitem {
    
} epitem;

typedef struct eventepoll {
    pthread_mutex_t lock;

    wait_queue_head_t wq;
    wait_queue_head_t poll_wait;

    list_head rdllist;
} eventepoll;