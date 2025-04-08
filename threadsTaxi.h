#ifndef THREADSTAXI_H
#define THREADSTAXI_H

#include "queue.h"
#include "mapa.h"
#include <pthread.h>
#include <stdbool.h>

#define TAXI_REFRESH_RATE 200000
#define TAXI_SPEED_FACTOR 5
#define MAX_PASSENGERS 100

#define MAX_TAXIS 10

typedef struct {
    int id;
    int x_calcada;
    int y_calcada;
    int x_rua;
    int y_rua;
} Passenger;

// Taxi structure
typedef struct {
    int id;
    int x, y;
    bool isFree;
    int currentPassenger;
    int destination_x; 
    int destination_y;
    MessageQueue queue;  
    pthread_mutex_t lock;
    MessageQueue* control_queue;
    MessageQueue* visualizerQueue; 
    pthread_t thread_id; 
    pthread_cond_t drop_cond; 
    bool drop_processed; 
} Taxi;

// Control center structure
typedef struct {
    int numPassengers;
    pthread_mutex_t lock;
    MessageQueue queue;
    MessageQueue* visualizerQueue; // Added visualizer queue reference
    Taxi* taxis[MAX_TAXIS];
    int numTaxis;
    Passenger* passengers[MAX_PASSENGERS]; // Passenger vector

} ControlCenter;


// Visualizer structure
typedef struct {
    int numQuadrados;
    int larguraRua;
    int larguraBorda;
    int minTamanho;
    int maxTamanho;
    int distanciaMinima;
    MessageQueue queue;
    MessageQueue* control_queue;
} Visualizer;

 void init_operations();

 pthread_t create_taxi_thread(Taxi* taxi);

 bool find_random_free_point(Mapa* mapa, int* random_x, int* random_y);


#endif