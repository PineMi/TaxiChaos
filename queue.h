#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>


// Message types
typedef enum {
    CREATE_PASSENGER,
    DELETE_PASSENGER,
    RESET_MAP,
    EXIT_PROGRAM,
    PATHFIND_REQUEST,
    RANDOM_REQUEST,
    ROUTE_PLAN,
    EXIT,
    STATUS_REQUEST,
    CREATE_TAXI,
    DESTROY_TAXI,
    SPAWN_TAXI,
    MOVE_TO,
    FINISH,
    PRINT_LOGICO,
    DROP,
    GOT_PASSENGER,
    CALL_TAXI
    
} MessageType;

// Message structure
typedef struct Message {
    MessageType type;
    struct Message* next;
    int data_x;
    int data_y;
    int extra_x;
    int extra_y;
    void* pointer;
} Message;

// Message queue structure
typedef struct {
    Message* head;
    Message* tail;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} MessageQueue;

typedef struct {
    int* solucaoX;         
    int* solucaoY;         
    int tamanho_solucao;   
} PathData;

 void init_queue(MessageQueue* queue);

 void enqueue_message(MessageQueue* queue, MessageType type, int x, int y, int extra_x, int extra_y, void* pointer);

 void priority_enqueue_message(MessageQueue* queue, MessageType type, int x, int y, int extra_x, int extra_y, void* pointer);

 Message* dequeue_message(MessageQueue* queue);

 void cleanup_queue(MessageQueue* queue);



#endif